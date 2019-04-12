#include "worker.hpp"

#include <iterator>
#include <algorithm>

#include <fmt/format.h>

#include "menu.hpp"
#include "logger.hpp"
#include "worker_session.hpp"
#include "json_utils.hpp"

namespace rt
{

worker::worker(worker_cfg cfg, int id_, net::io_context& ioc)
: id {id_}
, cfg {cfg.worker}
, ch_cfg {cfg.channel}
, ioc_ {ioc}
, timeouts {cfg.session}
, db {cfg.db, ioc, id_}
{
   net::post(ioc_, [this]() {init();});
}

void worker::init()
{
   auto handler = [this](auto data, auto const& req)
      { on_db_event(std::move(data), req); };

   db.set_on_msg_handler(handler);
   db.run();
}

void worker::on_db_get_menu(std::string const& data)
{
   auto const j_menu = json::parse(data);
   auto const is_empty = std::empty(menus);
   menus = j_menu.at("menus").get<std::vector<menu_elem>>();

   if (is_empty) {
      // The menus vector was empty. This happens only when the server
      // is started. We have to save the menu and retrieve the menu
      // messages from the database. It is important that the channels
      // are not created now since we did not retrieve the messages
      // from the database yet but we are already subscribed to the
      // menu channel.
      //
      // That means we will begin to receive messages and send them to
      // the users. Since these messages will be more recent then the
      // ones we are about to retrieve from the database, they will
      // update their last message counter. Then if they disconnect
      // and connect again the old messages will never be sent to
      // them. This is a very unlikely situation since the retrieval
      // of messages is fast.
      //
      // The channel creation will be delegated to the
      // *retrieve_menu_msgs* handler.

      // We may wish to check whether the menu received above is valid
      // before we proceed with the retrieval of the menu messages.
      // For example check if the menus have the correct number of
      // elements and the correct depth for each element.
      db.retrieve_menu_msgs(0);
      return;
   }

   // The menus is not empty which means we have received a new menu
   // and whould update the old. We only have to create the additional
   // channels and there is no need to retrieve messages from the
   // database. The new menu may contain additional channels that
   // should be created here.
   create_channels(menus);
}

void worker::create_channels(std::vector<menu_elem> const& menus_)
{
   auto const menu_codes = menu_elems_to_codes(menus_);

   auto created = 0;
   auto existed = 0;
   auto f = [&, this](auto const& comb)
   {
      auto const hash_code = to_channel_hash_code2(menu_codes, comb);

      if (channels.insert({hash_code, {}}).second)
         ++created;
      else
         ++existed;
   };

   visit_menu_codes(menu_codes, f);

   if (created != 0)
      log(loglevel::info, "W{0}: {1} channels created.", id, created);

   if (existed != 0)
      log(loglevel::info, "W{0}: {1} already existed.", id, existed);
}

void worker::on_db_menu_msgs(std::vector<std::string> const& msgs)
{
   // This function has two roles.
   //
   // 1. Create the channels if they do not already exist. This
   //    happens only when the server is started and the menu is
   //    retrieved for the first time. If the channels arealdy exist,
   //    that means a connection to the database that has been
   //    restablished and we requested all messages that we may have
   //    missed while we were offline.
   //
   // 2. Fill the channels with the menu messages.

   if (std::empty(channels))
      create_channels(menus);

   log( loglevel::info
      , "W{0}: {1} messages received from the database."
      , id, std::size(msgs));

   auto loader = [this](auto const& msg)
      { on_db_menu_msg(msg); };

   std::for_each(std::begin(msgs), std::end(msgs), loader);
}

void worker::on_db_menu_msg(std::string const& msg)
{
   auto const j = json::parse(msg);
   auto item = j.get<pub_item>();
   auto const code = to_channel_hash_code(item.to);
   auto const g = channels.find(code);
   if (g == std::end(channels)) {
      // This can happen if the subscription to the menu channel
      // happens before we receive the menu and generate the channels.
      // That is why it is important to update the last message id
      // only after this check.
      return;
   }

   if (item.id > last_menu_msg_id)
      last_menu_msg_id = item.id;

   g->second.broadcast(item, ch_cfg.max_posts);
}

void
worker::on_db_chat_msg( std::string const& user_id
                      , std::vector<std::string> msgs)
{
   auto const match = sessions.find(user_id);
   if (match == std::end(sessions)) {
      // The user went offline. We have to enqueue the message again.
      // This is difficult to test since the retrieval of messages
      // from the database is pretty fast.
      //
      // It is also such a rare situation that I won't care about
      // optimizing it by using move iterators.
      db.store_user_msg( user_id
                       , std::make_move_iterator(std::begin(msgs))
                       , std::make_move_iterator(std::end(msgs)));
      return;
   }

   if (auto s = match->second.lock()) {
      auto f = [s](auto o)
         { s->send(std::move(o), true); };

      std::for_each( std::make_move_iterator(std::begin(msgs))
                   , std::make_move_iterator(std::end(msgs))
                   , f);
      return;
   }
   
   // The user went offline but the session was not removed from
   // the map. This is perhaps not a bug but undesirable as we
   // do not have garbage collector for expired sessions.
   assert(false);
}

void
worker::on_db_event( std::vector<std::string> data
                   , redis::req_item const& req)
{
   switch (req.req)
   {
      case redis::request::unsol_user_msgs:
         on_db_chat_msg(req.user_id, std::move(data));
         break;

      case redis::request::get_menu:
         assert(std::size(data) == 1);
         on_db_get_menu(data.back());
         break;

      case redis::request::pub_counter:
         assert(std::size(data) == 1);
         on_db_pub_counter(data.back());
         break;

      case redis::request::publish:
         on_db_publish();
         break;

      case redis::request::menu_connect:
         on_db_menu_connect();
         break;

      case redis::request::menu_msgs:
         on_db_menu_msgs(data);
         break;

      case redis::request::unsol_publish:
         assert(std::size(data) == 1);
         on_db_menu_msg(data.back());
         break;

      default:
         break;
   }
}

void worker::on_db_menu_connect()
{
   // There are two situations we have to distinguish here.
   //
   // 1. The server has started and still does not have the menu, we
   //    have to retrieve it.
   //
   // 2. The connection to the database (redis) has been lost and
   //    restablished. In this case we have to retrieve all the
   //    messages that may have arrived while we were offline.

   if (std::empty(menus)) {
      db.async_retrieve_menu();
   } else {
      db.retrieve_menu_msgs(1 + last_menu_msg_id);
   }
}

void worker::on_db_publish()
{
   pub_wait_queue.pop();
   if (!std::empty(pub_wait_queue))
      db.request_pub_id();
}

void worker::on_db_pub_counter(std::string const& pub_id_str)
{
   auto const pub_id = std::stoi(pub_id_str);
   pub_wait_queue.front().item.id = pub_id;
   json const j_item = pub_wait_queue.front().item;
   db.pub_menu_msg(j_item.dump(), pub_id);

   // It is important that the publisher receives this message before
   // any user sends him a user message about the post. He needs a
   // pub_id to know to which post the user refers to.
   json ack;
   ack["cmd"] = "publish_ack";
   ack["result"] = "ok";
   ack["id"] = pub_wait_queue.front().item.id;
   auto ack_str = ack.dump();

   if (auto s = pub_wait_queue.front().session.lock()) {
      s->send(std::move(ack_str), true);
      return;
   }

   // If we get here the user is not online anymore. This should be a
   // very rare situation since requesting an id takes milliseconds.
   // We can simply store his ack in the database for later retrieval
   // when the user connects again. It shall be difficult to test
   // this.

   std::initializer_list<std::string> param = {ack_str};

   db.store_user_msg( std::move(pub_wait_queue.front().user_id)
                    , std::make_move_iterator(std::begin(param))
                    , std::make_move_iterator(std::end(param)));
}

ev_res
worker::on_user_register( json const& j
                        , std::shared_ptr<worker_session> s)
{
   auto const from = j.at("from").get<std::string>();

   // TODO: Replace this with a query to the database.
   if (sessions.find(from) != std::end(sessions)) {
      // The user already exists in the system.
      json resp;
      resp["cmd"] = "register_ack";
      resp["result"] = "fail";
      s->send(resp.dump(), false);
      return ev_res::register_fail;
   }

   s->set_id(from);

   // TODO: Use a random number generator with six digits.
   s->set_code("8347");

   json resp;
   resp["cmd"] = "register_ack";
   resp["result"] = "ok";
   resp["menus"] = menus;
   s->send(resp.dump(), false);
   return ev_res::register_ok;
}

ev_res
worker::on_user_login( json const& j
                     , std::shared_ptr<worker_session> s)
{
   auto const from = j.at("from").get<std::string>();

   auto const new_user = sessions.insert({from, s});
   if (!new_user.second) {
      // The user is already logged on the system. We do not allow
      // this yet.
      json resp;
      resp["cmd"] = "auth_ack";
      resp["result"] = "fail";
      s->send(resp.dump(), false);
      return ev_res::login_fail;
   }

   // TODO: Query the database to validate the session.
   //if (from != s->get_id()) {
   //   // Incorrect id.
   //   json resp;
   //   resp["cmd"] = "auth_ack";
   //   resp["result"] = "fail";
   //   s->send(resp.dump());
   //   return ev_res::login_fail;
   //}

   s->set_id(from);
   s->promote();
   assert(s->is_auth());

   db.subscribe_to_chat_msgs(s->get_id());
   db.retrieve_chat_msgs(s->get_id());

   json resp;
   resp["cmd"] = "auth_ack";
   resp["result"] = "ok";

   auto const user_versions =
      j.at("menu_versions").get<std::vector<int>>();

   // Cache this value?
   auto const server_versions = read_versions(menus);

   auto const b =
      std::lexicographical_compare( std::begin(user_versions)
                                  , std::end(user_versions)
                                  , std::begin(server_versions)
                                  , std::end(server_versions));

   if (b)
      resp["menus"] = menus;

   s->send(resp.dump(), false);

   return ev_res::login_ok;
}

ev_res
worker::on_user_code_confirm( json const& j
                            , std::shared_ptr<worker_session> s)
{
   auto const from = j.at("from").get<std::string>();
   auto const code = j.at("code").get<std::string>();

   if (code != s->get_code()) {
      json resp;
      resp["cmd"] = "code_confirmation_ack";
      resp["result"] = "fail";
      s->send(resp.dump(), false);
      return ev_res::code_confirmation_fail;
   }

   s->promote();

   // Inserts the user in the system.
   assert(!std::empty(s->get_id()));
   auto const new_user = sessions.insert({from, s});
   assert(s->is_auth());

   // This would be odd. The entry already exists on the index map
   // which means we did something wrong in the register command.
   assert(new_user.second);

   db.subscribe_to_chat_msgs(s->get_id());

   json resp;
   resp["cmd"] = "code_confirmation_ack";
   resp["result"] = "ok";
   s->send(resp.dump(), false);
   return ev_res::code_confirmation_ok;
}

ev_res
worker::on_user_subscribe( json const& j
                         , std::shared_ptr<worker_session> s)
{
   auto const menu_codes =
      j.at("channels").get<menu_code_type>();

   auto n_channels = 0;
   std::vector<pub_item> items;

   auto psession = s->get_proxy_session(true);

   // Works only for two menus with depth 2.
   auto f = [&, this](auto const& comb)
   {
      auto const hash_code = to_channel_hash_code2(menu_codes, comb);
      auto const g = channels.find(hash_code);
      assert(g != std::end(channels));
      if (g == std::end(channels))
         return;

      // TODO: Change 0 with the latest id the user has received.
      g->second.retrieve_pub_items(0, std::back_inserter(items));
      g->second.add_member(psession, ch_cfg.cleanup_rate);
      ++n_channels;
   };

   visit_menu_codes(menu_codes, f, ch_cfg.max_sub);

   if (ssize(items) > cfg.max_menu_msg_on_sub) {
      auto comp = [](auto const& a, auto const& b)
         { return a.id > b.id; };

      std::nth_element( std::begin(items)
                      , std::begin(items) + cfg.max_menu_msg_on_sub
                      , std::end(items)
                      , comp);

      items.erase( std::begin(items) + cfg.max_menu_msg_on_sub
                 , std::end(items));
   }

   json resp;
   resp["cmd"] = "subscribe_ack";
   resp["result"] = "ok";
   s->send(resp.dump(), false);

   json j_pub;
   j_pub["cmd"] = "publish";
   j_pub["items"] = items;
   s->send(j_pub.dump(), false);
   return ev_res::subscribe_ok;
}

ev_res
worker::on_user_publish(json j, std::shared_ptr<worker_session> s)
{
   // TODO: Remove the restriction below that the items vector have
   // size one.
   auto items = j.at("items").get<std::vector<pub_item>>();

   // Before we request a new pub id, we have to check that the post
   // is valid and will not be refused later. This prevents the pub
   // items from being incremented on an invalid message. May be
   // overkill since we do not expect the app to contain so severe
   // bugs. Also, the session at this point has been authenticated and
   // can be thrusted.

   // The channel code has the form [[[1, 2]], [[2, 3, 4]], [[1, 2]]]
   // where each array in the outermost array refers to one menu.
   auto const hash_code = to_channel_hash_code(items.front().to);

   auto const g = channels.find(hash_code);
   if (g == std::end(channels) || std::size(items) != 1) {
      // This is a non-existing channel. Perhaps the json command was
      // sent with the wrong information signaling a logic error in
      // the app. Sending a fail ack back to the app is useful to
      // debug it? See redis::request::unsolicited_publish Enable only
      // for debug. But ... It may prevent the traffic of invalid
      // messages in redis. But invalid messages should never happen.
      json resp;
      resp["cmd"] = "publish_ack";
      resp["result"] = "fail";
      s->send(resp.dump(), false);
      return ev_res::publish_fail;
   }

   auto const is_empty = std::empty(pub_wait_queue);

   pub_wait_queue.push({s, std::move(items.front()), items.front().from});

   if (is_empty)
      db.request_pub_id();

   return ev_res::publish_ok;
}

void worker::on_session_dtor( std::string const& id
                            , std::vector<std::string> msgs)
{
   // TODO: This function is called on the destructor on the server
   // session. Think where should we catch exceptions.

   auto const match = sessions.find(id);
   if (match == std::end(sessions)) {
      // This is a bug, all autheticated sessions should be in the
      // sessions map.
      assert(false);
      return;
   }

   sessions.erase(match);
   db.unsubscribe_to_chat_msgs(id);

   if (!std::empty(msgs)) {
      db.store_user_msg( std::move(id)
                       , std::make_move_iterator(std::begin(msgs))
                       , std::make_move_iterator(std::end(msgs)));
   }
}

ev_res
worker::on_chat_msg( std::string msg, json const& j
                   , std::shared_ptr<worker_session> s)
{
   // TODO: Search the sessions map if the user is online and in this
   // node and send him his message directly to avoid overloading the
   // redis server. This would be a big optimization in the case of
   // small number of nodes.

   auto to = j.at("to").get<std::string>();

   std::initializer_list<std::string> param = {msg};

   db.store_user_msg( std::move(to)
                    , std::make_move_iterator(std::begin(param))
                    , std::make_move_iterator(std::end(param)));

   json ack;
   ack["cmd"] = "user_msg_server_ack";
   ack["result"] = "ok";
   s->send(ack.dump(), false);
   return ev_res::chat_msg_ok;
}

void worker::shutdown()
{
   log( loglevel::notice
      , "W{0}: Shutting down has been requested."
      , id);

   log( loglevel::notice
      , "W{0}: {1} sessions will be closed."
      , id, std::size(sessions));

   auto f = [](auto o)
   {
      if (auto s = o.second.lock())
         s->shutdown();
   };

   std::for_each(std::begin(sessions), std::end(sessions), f);

   db.disconnect();
}

ev_res
worker::on_message(std::shared_ptr<worker_session> s, std::string msg)
{
   auto const j = json::parse(msg);
   auto const cmd = j.at("cmd").get<std::string>();

   if (s->is_waiting_auth()) {
      if (cmd == "register")
         return on_user_register(j, s);

      if (cmd == "auth")
         return on_user_login(j, s);

      return ev_res::unknown;
   }

   if (s->is_waiting_code()) {
      if (cmd == "code_confirmation")
         return on_user_code_confirm(j, s);

      return ev_res::unknown;
   }

   if (s->is_auth()) {
      if (cmd == "subscribe")
         return on_user_subscribe(j, s);

      if (cmd == "publish")
         return on_user_publish(std::move(j), s);

      if (cmd == "user_msg")
         return on_chat_msg(std::move(msg), j, s);

      return ev_res::unknown;
   }

   return ev_res::unknown;
}

}


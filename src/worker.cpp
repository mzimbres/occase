#include "worker.hpp"

#include <string>
#include <iterator>
#include <algorithm>
#include <functional>

#include <fmt/format.h>

#include "menu.hpp"
#include "utils.hpp"
#include "logger.hpp"
#include "worker_session.hpp"
#include "json_utils.hpp"

namespace rt
{

//__________________________________________________________________________
std::ostream& operator<<(std::ostream& os, worker_stats const& stats)
{
   os << stats.number_of_sessions
      << ";"
      << stats.worker_post_queue_size
      << ";"
      << stats.worker_reg_queue_size
      << ";"
      << stats.worker_login_queue_size
      << ";"
      << stats.db_post_queue_size
      << ";"
      << stats.db_chat_queue_size;

   return os;
}

void add(worker_stats& a, worker_stats const& b)
{
   a.number_of_sessions      += b.number_of_sessions;
   a.worker_post_queue_size  += b.worker_post_queue_size;
   a.worker_reg_queue_size   += b.worker_reg_queue_size;
   a.worker_login_queue_size += b.worker_login_queue_size;
   a.db_post_queue_size      += b.db_post_queue_size;
   a.db_chat_queue_size      += b.db_chat_queue_size;
}

//__________________________________________________________________________
worker::worker(worker_cfg cfg, int id_, net::io_context& ioc)
: ioc_ {ioc}
, id {id_}
, cfg {cfg.worker}
, ch_cfg {cfg.channel}
, ws_ss_timeouts_ {cfg.timeouts}
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

void worker::on_db_menu(std::string const& data)
{
   auto const j_menu = json::parse(data);
   auto const is_empty = std::empty(menu);
   menu = j_menu.at("menus").get<std::vector<menu_elem>>();

   if (is_empty) {
      // The menu vector was empty. This happens only when the server
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
      // *retrieve_posts* handler.

      // We may wish to check whether the menu received above is valid
      // before we proceed with the retrieval of the menu messages.
      // For example check if the menu have the correct number of
      // elements and the correct depth for each element.
      db.retrieve_posts(0);
      return;
   }

   // The menu is not empty which means we have received a new menu
   // and whould update the old. We only have to create the additional
   // channels and there is no need to retrieve messages from the
   // database. The new menu may contain additional channels that
   // should be created here.
   create_channels(menu);
}

void worker::create_channels(std::vector<menu_elem> const& menus_)
{
   auto const menu_codes = menu_elems_to_codes(menus_);

   auto created = 0;
   auto existed = 0;
   auto f = [&, this](auto const& comb)
   {
      auto const hash_code =
         to_hash_code_impl(menu_codes, std::cbegin(comb));

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

void worker::on_db_posts(std::vector<std::string> const& msgs)
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
      create_channels(menu);

   log( loglevel::info, "W{0}: {1} messages received from the database."
      , id, std::size(msgs));

   auto loader = [this](auto const& msg)
      { on_db_post(msg); };

   std::for_each(std::begin(msgs), std::end(msgs), loader);
}

void worker::on_db_post(std::string const& msg)
{
   auto const j = json::parse(msg);
   auto const item = j.get<post>();
   auto const hash_code = to_hash_code(item.to);
   auto const g = channels.find(hash_code);
   if (g == std::end(channels)) {
      // This can happen if the subscription to the menu channel
      // happens before we receive the menu and generate the channels.
      // That is why it is important to update the last message id
      // only after this check.
      return;
   }

   if (item.id > last_post_id)
      last_post_id = item.id;

   g->second.broadcast(item, ch_cfg.max_posts);
}

void worker::on_db_user_id(std::string const& id)
{
   assert(!std::empty(reg_queue));

   if (auto session = reg_queue.front().session.lock()) {
      reg_queue.front().pwd = pwdgen(cfg.pwd_size);

      // We store a hashed version of the password in the database.
      auto const hashed_pwd = hash_func(reg_queue.front().pwd);
      auto const hashed_pwd_str = std::to_string(hashed_pwd);
      db.register_user(id, hashed_pwd_str);
      session->set_id(id);
   } else {
      // The user is not online anymore. The requested id is lost.
      // Very unlikely to happen since the communication with redis is
      // very fast.
      reg_queue.pop();
   }
}

void worker::on_db_user_data(std::vector<std::string> const& data)
{
   assert(!std::empty(login_queue));

   if (auto s = login_queue.front().session.lock()) {
      // In the db we store only hashed pwds.
      auto const hashed_pwd = hash_func(login_queue.front().pwd);
      auto const hashed_pwd_str = std::to_string(hashed_pwd);
      if (data.back() != hashed_pwd_str) {
         // Incorrect pwd.
         json resp;
         resp["cmd"] = "login_ack";
         resp["result"] = "fail";
         s->send(resp.dump(), false);
         s->shutdown();
      } else {
         // We do not expect the insertion to fail, at the moment
         // however I do not see a reason to treat error.
         sessions.insert({s->get_id(), s});

         db.subscribe_to_chat_msgs(s->get_id());
         db.retrieve_chat_msgs(s->get_id());

         json resp;
         resp["cmd"] = "login_ack";
         resp["result"] = "ok";
         if (login_queue.front().send_menu)
            resp["menus"] = menu;

         s->send(resp.dump(), false);
      }
   } else {
      // The user is not online anymore. The requested id is lost.
      // Very unlikely to happen since the communication with redis is
      // very fast.
   }

   login_queue.pop();
}

void worker::on_db_register()
{
   assert(!std::empty(reg_queue));
   if (auto session = reg_queue.front().session.lock()) {
      auto const& id = session->get_id();
      db.subscribe_to_chat_msgs(id);
      auto const new_user = sessions.insert({id, session});

      // It would be a bug if this id were already in the map since we
      // have just registered it.
      assert(new_user.second);

      json resp;
      resp["cmd"] = "register_ack";
      resp["result"] = "ok";
      resp["id"] = id;
      resp["password"] = reg_queue.front().pwd;
      resp["menus"] = menu;
      session->send(resp.dump(), false);
   } else {
      // The user is not online anymore. The requested id is lost.
   }

   reg_queue.pop();
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
      db.store_chat_msg( user_id
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
      case redis::request::menu:
         assert(std::size(data) == 1);
         on_db_menu(data.back());
         break;

      case redis::request::chat_messages:
         on_db_chat_msg(req.user_id, std::move(data));
         break;

      case redis::request::post:
         on_db_publish();
         break;

      case redis::request::posts:
         on_db_posts(data);
         break;

      case redis::request::post_id:
         assert(std::size(data) == 1);
         on_db_post_id(data.back());
         break;

      case redis::request::user_id:
         assert(std::size(data) == 1);
         on_db_user_id(data.back());
         break;

      case redis::request::user_data:
         on_db_user_data(data);
         break;

      case redis::request::register_user:
         on_db_register();
         break;

      case redis::request::menu_connect:
         on_db_menu_connect();
         break;

      case redis::request::unsol_publish:
         assert(std::size(data) == 1);
         on_db_post(data.back());
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

   if (std::empty(menu)) {
      db.retrieve_menu();
   } else {
      db.retrieve_posts(1 + last_post_id);
   }
}

void worker::on_db_publish()
{
   // We do not need this function at the moment.
}

void worker::on_db_post_id(std::string const& post_id_str)
{
   auto const post_id = std::stoi(post_id_str);
   post_queue.front().item.id = post_id;
   json const j_item = post_queue.front().item;
   db.post(j_item.dump(), post_id);

   // It is important that the publisher receives this message before
   // any user sends him a user message about the post. He needs a
   // post_id to know to which post the user refers to.
   json ack;
   ack["cmd"] = "publish_ack";
   ack["result"] = "ok";
   ack["id"] = post_queue.front().item.id;
   auto ack_str = ack.dump();

   if (auto s = post_queue.front().session.lock()) {
      s->send(std::move(ack_str), true);
   } else {
      // If we get here the user is not online anymore. This should be a
      // very rare situation since requesting an id takes milliseconds.
      // We can simply store his ack in the database for later retrieval
      // when the user connects again. It shall be difficult to test
      // this.

      log( loglevel::debug
         , "W{0}/on_db_post_id: Sending ack to the database."
         , id);

      std::initializer_list<std::string> param = {ack_str};

      db.store_chat_msg( std::move(post_queue.front().item.from)
                       , std::make_move_iterator(std::begin(param))
                       , std::make_move_iterator(std::end(param)));
   }

   post_queue.pop();
}

ev_res
worker::on_app_login( json const& j
                    , std::shared_ptr<worker_session> s)
{
   auto const user = j.at("user").get<std::string>();
   s->set_id(user);

   auto const user_versions =
      j.at("menu_versions").get<std::vector<int>>();

   // Cache this value?
   auto const server_versions = read_versions(menu);

   auto const b =
      std::lexicographical_compare( std::begin(user_versions)
                                  , std::end(user_versions)
                                  , std::begin(server_versions)
                                  , std::end(server_versions));

   auto const pwd = j.at("password").get<std::string>();

   // We do not have to serialize the calls to retrieve_user_data
   // since this will be done by the db object.
   login_queue.push({s, pwd, b});
   db.retrieve_user_data(s->get_id());

   return ev_res::login_ok;
}

ev_res
worker::on_app_register( json const& j
                       , std::shared_ptr<worker_session> s)
{
   reg_queue.push({s, {}});
   db.request_user_id();
   return ev_res::register_ok;
}

ev_res
worker::on_app_subscribe( json const& j
                        , std::shared_ptr<worker_session> s)
{
   auto const menu_codes =
      j.at("channels").get<menu_code_type>();

   auto const app_last_post_id =
      j.at("last_post_id").get<int>();

   auto n_channels = 0;
   std::vector<post> items;

   auto psession = s->get_proxy_session(true);

   auto f = [&, this](auto const& comb)
   {
      auto const hash_code =
         to_hash_code_impl(menu_codes, std::cbegin(comb));
      auto const g = channels.find(hash_code);
      if (g == std::end(channels)) {
         log( loglevel::debug
            , "W{0}/on_app_subscribe: Invalid channels." , id);
         return;
      }

      g->second.retrieve_pub_items( app_last_post_id
                                  , std::back_inserter(items));
      g->second.add_member(psession, ch_cfg.cleanup_rate);
      ++n_channels;
   };

   visit_menu_codes(menu_codes, f, ch_cfg.max_sub);

   if (ssize(items) > cfg.max_posts_on_sub) {
      auto comp = [](auto const& a, auto const& b)
         { return a.id > b.id; };

      std::nth_element( std::begin(items)
                      , std::begin(items) + cfg.max_posts_on_sub
                      , std::end(items)
                      , comp);

      items.erase( std::begin(items) + cfg.max_posts_on_sub
                 , std::end(items));
   }

   json resp;
   resp["cmd"] = "subscribe_ack";
   resp["result"] = "ok";
   s->send(resp.dump(), false);

   if (!std::empty(items)) {
      json j_pub;
      j_pub["cmd"] = "publish";
      j_pub["items"] = items;
      s->send(j_pub.dump(), false);
   }

   return ev_res::subscribe_ok;
}

ev_res
worker::on_app_publish(json j, std::shared_ptr<worker_session> s)
{
   // Consider remove the restriction below that the items vector have
   // size one.
   auto items = j.at("items").get<std::vector<post>>();

   // Before we request a new pub id, we have to check that the post
   // is valid and will not be refused later. This prevents the pub
   // items from being incremented on an invalid message. May be
   // overkill since we do not expect the app to contain so severe
   // bugs. Also, the session at this point has been authenticated and
   // can be thrusted.

   // The channel code has the form [[[1, 2]], [[2, 3, 4]], [[1, 2]]]
   // where each array in the outermost array refers to one menu.
   auto const hash_code = to_hash_code(items.front().to);

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

   // It is important to not thrust the *from* field in the json
   // command.
   items.front().from = s->get_id();
   post_queue.push({s, items.front()});
   db.request_post_id();
   return ev_res::publish_ok;
}

void worker::on_session_dtor( std::string const& user_id
                            , std::vector<std::string> msgs)
{
   auto const match = sessions.find(user_id);
   if (match == std::end(sessions))
      return;

   sessions.erase(match);
   db.unsubscribe_to_chat_msgs(user_id);

   if (!std::empty(msgs)) {
      log( loglevel::debug
         , "W{0}/on_session_dtor: Storing messages from {1} in db."
         , id, user_id);

      db.store_chat_msg( std::move(user_id)
                       , std::make_move_iterator(std::begin(msgs))
                       , std::make_move_iterator(std::end(msgs)));
   }
}

ev_res
worker::on_app_chat_msg(json j, std::shared_ptr<worker_session> s)
{
   // Consider searching the sessions map if the user in this worker
   // and send him his message directly to avoid overloading the redis
   // server. This would be a big optimization in the case of small
   // number of workers.

   // Do not thrust the from field in the json command before sending
   // to the database.
   j["from"] = s->get_id();
   auto msg = j.dump();
   std::initializer_list<std::string> param = {msg};

   auto to = j.at("to").get<std::string>();
   db.store_chat_msg( std::move(to)
                    , std::make_move_iterator(std::begin(param))
                    , std::make_move_iterator(std::end(param)));

   json ack;
   ack["cmd"] = "message_server_ack";
   ack["result"] = "ok";
   s->send(ack.dump(), false);
   return ev_res::chat_msg_ok;
}

void worker::shutdown()
{
   log( loglevel::notice, "W{0}: Shutting down has been requested."
      , id);

   log( loglevel::notice, "W{0}: {1} sessions will be closed."
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
   auto j = json::parse(msg);
   auto const cmd = j.at("cmd").get<std::string>();

   if (s->is_logged_in()) {
      if (cmd == "subscribe")
         return on_app_subscribe(j, s);

      if (cmd == "publish")
         return on_app_publish(std::move(j), s);

      if (cmd == "message")
         return on_app_chat_msg(std::move(j), s);
   } else {
      if (cmd == "login")
         return on_app_login(j, s);

      if (cmd == "register")
         return on_app_register(j, s);
   }

   return ev_res::unknown;
}

worker_stats worker::get_stats() const noexcept
{
   worker_stats wstats {};

   wstats.number_of_sessions = ws_ss_stats_.number_of_sessions;
   wstats.worker_post_queue_size = ssize(post_queue);
   wstats.worker_reg_queue_size = ssize(reg_queue);
   wstats.worker_login_queue_size = ssize(login_queue);
   wstats.db_post_queue_size = db.get_post_queue_size();
   wstats.db_chat_queue_size = db.get_chat_queue_size();

   return wstats;
}

//________________________________________________________________________

worker_arena::worker_arena(worker_cfg const& cfg, int i)
: id_ {i}
, signals_ {ioc_, SIGINT, SIGTERM}
, worker_ {cfg, i, ioc_}
, thread_ { std::thread {[this](){ run();}} }
{
   auto handler = [this](auto const& ec, auto n)
      { on_signal(ec, n); };

   signals_.async_wait(handler);
}

worker_arena::~worker_arena()
{
   thread_.join();
}

void worker_arena::on_signal(boost::system::error_code const& ec, int n)
{
   if (ec) {
      if (ec == net::error::operation_aborted) {
         // No signal occurred, the handler was canceled. We just
         // leave.
         return;
      }

      log( loglevel::crit
         , "W{0}/worker_arena::on_signal: Unhandled error '{1}'"
         , id_, ec.message());

      return;
   }

   log( loglevel::notice
      , "W{0}/worker_arena::on_signal: Signal {1} has been captured."
      , id_, n);

   worker_.shutdown();
}

void worker_arena::run() noexcept
{
   try {
      ioc_.run();
   } catch (std::exception const& e) {
      log( loglevel::notice
         , "W{0}/worker_arena::run: {1}", id_, e.what());
   }
}

}


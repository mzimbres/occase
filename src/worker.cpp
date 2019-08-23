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

void add(worker_stats& a, worker_stats const& b) noexcept
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
, worker_id {id_}
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
   auto const is_empty = is_menu_empty(menu);
   menu = j_menu.at("menus").get<menu_elems_array_type>();

   if (is_empty) {
      // The menu vector is empty. This happens only when the server
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
   // and should update the old. We only have to create the additional
   // channels and there is no need to retrieve messages from the
   // database. The new menu may contain additional channels that
   // should be created here.
   create_channels(menu);
}

void worker::create_channels(menu_elems_array_type const& me)
{
   auto const menu_channels = menu_elems_to_codes(me.back());

   auto created = 0;
   auto existed = 0;
   auto f = [&, this](auto const& code)
   {
      auto const hash_code = to_hash_code(code, me.back().depth);

      if (channels.insert({hash_code, {}}).second)
         ++created;
      else
         ++existed;
   };

   std::for_each( std::cbegin(menu_channels)
                , std::cend(menu_channels)
                , f);

   if (created != 0)
      log(loglevel::info, "W{0}: {1} channels created.", worker_id, created);

   if (existed != 0)
      log(loglevel::info, "W{0}: {1} already existed.", worker_id, existed);
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
      , worker_id, std::size(msgs));

   auto loader = [this](auto const& msg)
      { on_db_channel_msg(msg); };

   std::for_each(std::begin(msgs), std::end(msgs), loader);
}

void worker::on_db_channel_msg(std::string const& msg)
{
   // NOTE: Later we may have to improve how we decide if a message is
   // a post or a command, like deleter, specially the number of
   // commands that sent from workers to workers increases.

   // This function has two roles, deal with the delete command and
   // new posts.
   auto const j = json::parse(msg);
   auto const channel = j.at("to").get<menu_code_type>();
   auto const hash_code = to_hash_code(
      channel.at(1).at(0),
      menu.back().depth);

   auto const g = channels.find(hash_code);
   if (g == std::end(channels)) {
      // This can happen if the subscription to the menu channel
      // happens before we receive the menu and generate the channels.
      // That is why it is important to update the last message id
      // only after this check.
      log(loglevel::debug, "W{0}: Channel could not be found.", worker_id);
      return;
   }

   if (j.contains("cmd")) {
      auto const post_id = j.at("id").get<int>();
      auto const from = j.at("from").get<std::string>();
      auto const r = g->second.remove_post(post_id, from);
      auto const* str = "W{0}: Failed to remove post {1}.";
      if (r)
         str = "W{0}: Post {1} successfully removed.";

      log(loglevel::notice, str , worker_id, post_id);
      return;
   }

   auto const item = j.get<post>();

   if (item.id > last_post_id)
      last_post_id = item.id;

   g->second.broadcast(item, ch_cfg.max_posts, menu.at(0).depth);

   // The maximum number of posts that can be stored on the products
   // channel should be higher than on the specialied channels. I do
   // not know which number is good enough. TODO move this decision to
   // the config file.
   // NOTE: A number that is too high may compromize scalability.
   product_channel.broadcast(
      item,
      cfg.max_posts_on_sub, menu.at(0).depth);
}

void worker::on_db_user_id(std::string const& id)
{
   assert(!std::empty(reg_queue));

   if (auto session = reg_queue.front().session.lock()) {
      reg_queue.front().pwd = pwdgen(cfg.pwd_size);

      // We store a hashed version of the password in the database.
      auto const digest = make_hex_digest(reg_queue.front().pwd);
      db.register_user(id, digest);
      session->set_id(id);
   } else {
      // The user is not online anymore. The requested id is lost.
      // Very unlikely to happen since the communication with redis is
      // very fast.
      reg_queue.pop();
   }
}

void
worker::on_db_user_data(std::vector<std::string> const& data) noexcept
{
   try {
      assert(!std::empty(login_queue));

      if (auto s = login_queue.front().session.lock()) {
         auto const digest = make_hex_digest(login_queue.front().pwd);
         if (data.back() != digest) {
            // Incorrect pwd.
            json resp;
            resp["cmd"] = "login_ack";
            resp["result"] = "fail";
            s->send(resp.dump(), false);
            s->shutdown();
            log( loglevel::debug
               , "W{0}/on_db_user_data: login_ack failed1 for {1}:{2}."
               , worker_id, s->get_id(), login_queue.front().pwd);
         } else {
            auto const ss = sessions.insert({s->get_id(), s});
            if (!ss.second) {
               // Somebody is exploiting the server? Each app gets a
               // different id and therefore there should never be
               // more than one session with the same id. For now, I
               // will simply override the old session and disregard
               // any pending messages.

               // The old session has to be shutdown first
               if (auto old_ss = ss.first->second.lock()) {
                  old_ss->shutdown();
                  // We have to prevent the cleanup operation when its
                  // destructor is called. That would cause the new
                  // session to be removed from the map again.
                  old_ss->set_id("");
               } else {
                  // Awckward, the old session has already expired and
                  // we did not remove it from the map. It should be
                  // fixed.
               }

               ss.first->second = s;
            }

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
   } catch (...) {
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

// TODO: Make this function noexcept.
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
         on_db_channel_msg(data.back());
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

   if (is_menu_empty(menu)) {
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
   ack["date"] = post_queue.front().item.date;
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
         , worker_id);

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

   auto const menu_versions =
      j.at("menu_versions").get<std::vector<int>>();

   // Cache this value?
   auto const server_versions = read_versions(menu);

   auto const needs_new_menu =
      std::lexicographical_compare( std::begin(menu_versions)
                                  , std::end(menu_versions)
                                  , std::begin(server_versions)
                                  , std::end(server_versions));

   auto const pwd = j.at("password").get<std::string>();

   // We do not have to serialize the calls to retrieve_user_data
   // since this will be done by the db object.
   login_queue.push({s, pwd, needs_new_menu});
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

ev_res worker::on_app_subscribe(
   json const& j,
   std::shared_ptr<worker_session> s)
{
   auto const menu_channels = j.at("channels").get<menu_code_type>();
   auto const filter = j.at("filter").get<std::uint64_t>();
   auto const app_last_post_id = j.at("last_post_id").get<int>();

   s->set_filter(filter);
   s->set_filter(menu_channels.at(0), menu.front().depth);
   auto psession = s->get_proxy_session(true);

   std::vector<post> items;

   // If the second channels are empty, the app wants posts from all
   // channels, otherwise we have to traverse the individual channels.
   if (has_empty_products(menu_channels)) {
      product_channel.add_member(psession, ch_cfg.cleanup_rate);
      product_channel.retrieve_pub_items(app_last_post_id,
         std::back_inserter(items));
   } else {
      auto invalid_count = 0;
      auto f = [&, this](auto const& code)
      {
         auto const hash_code = to_hash_code(code, menu.at(1).depth);
         auto const g = channels.find(hash_code);
         if (g == std::end(channels)) {
            ++invalid_count;
            return;
         }

         g->second.add_member(psession, ch_cfg.cleanup_rate);
         g->second.retrieve_pub_items(app_last_post_id,
            std::back_inserter(items));
      };

      auto const d = std::min( ssize(menu_channels.at(1))
                             , ch_cfg.max_sub);

      std::for_each( std::cbegin(menu_channels.at(1))
                   , std::cbegin(menu_channels.at(1)) + d
                   , f);

      if (invalid_count != 0) {
         log( loglevel::debug
            , "W{0}/on_app_subscribe: Invalid channel(s)."
            , invalid_count);
      }
   }

   if (ssize(items) > cfg.max_posts_on_sub) {
      std::nth_element( std::begin(items)
                      , std::begin(items) + cfg.max_posts_on_sub
                      , std::end(items));

      items.erase( std::begin(items) + cfg.max_posts_on_sub
                 , std::end(items));
   }

   json resp;
   resp["cmd"] = "subscribe_ack";
   resp["result"] = "ok";
   s->send(resp.dump(), false);

   if (!std::empty(items)) {
      json j_pub;
      j_pub["cmd"] = "post";
      j_pub["items"] = items;
      s->send(j_pub.dump(), false);
   }

   return ev_res::subscribe_ok;
}

ev_res worker::on_app_del_post(json j, std::shared_ptr<worker_session> s)
{
   // A post should be deleted from the database as well as from each
   // worker, so that users do not receive posts from products that
   // are already sold.  To delete from the workers it is enough to
   // broadcast a delete command. Each channel should check if the
   // delete command belongs indeed to the user that wants to delete
   // it.
   auto const post_id = j.at("id").get<int>();
   j["from"] = s->get_id();
   db.remove_post(post_id, j.dump());

   json resp;
   resp["cmd"] = "delete_ack";
   resp["result"] = "ok";
   s->send(resp.dump(), true);

   return ev_res::delete_ok;
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
   auto const hash_code =
      to_hash_code( items.front().to.at(1).at(0)
                  , menu.back().depth);

   auto const g = channels.find(hash_code);
   if (g == std::end(channels) || std::size(items) != 1) {
      // This is a non-existing channel. Perhaps the json command was
      // sent with the wrong information signaling a logic error in
      // the app. Sending a fail ack back to the app is useful to
      // debug it? See redis::request::unsolicited_publish Enable only
      // for debug. But ... It may prevent the traffic of invalid
      // messages in redis.
      json resp;
      resp["cmd"] = "publish_ack";
      resp["result"] = "fail";
      s->send(resp.dump(), false);
      return ev_res::publish_fail;
   }

   using namespace std::chrono;

   // It is important to not thrust the *from* field in the json
   // command.
   items.front().from = s->get_id();
   auto const tse = system_clock::now().time_since_epoch();
   items.front().date = duration_cast<milliseconds>(tse).count();
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
         , worker_id, user_id);

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

   j["from"] = s->get_id();
   auto msg = j.dump();
   std::initializer_list<std::string> param = {msg};

   auto const to = j.at("to").get<std::string>();
   db.store_chat_msg( to
                    , std::make_move_iterator(std::begin(param))
                    , std::make_move_iterator(std::end(param)));

   auto const is_sender_post = j.at("is_sender_post").get<bool>();
   auto const post_id = j.at("post_id").get<int>();
   json ack;
   ack["cmd"] = "message";
   ack["from"] = to;
   ack["is_sender_post"] = !is_sender_post;
   ack["post_id"] = post_id;
   ack["type"] = "server_ack";
   ack["result"] = "ok";
   s->send(ack.dump(), false);
   return ev_res::chat_msg_ok;
}

void worker::shutdown()
{
   log( loglevel::notice, "W{0}: Shutting down has been requested."
      , worker_id);

   log( loglevel::notice, "W{0}: {1} sessions will be closed."
      , worker_id, std::size(sessions));

   auto f = [](auto o)
   {
      if (auto s = o.second.lock())
         s->shutdown();
   };

   std::for_each(std::begin(sessions), std::end(sessions), f);

   db.disconnect();
}

ev_res worker::on_app( std::shared_ptr<worker_session> s
                     , std::string msg) noexcept
{
   try {
      auto j = json::parse(msg);
      auto const cmd = j.at("cmd").get<std::string>();

      if (s->is_logged_in()) {
         if (cmd == "message")   return on_app_chat_msg(std::move(j), s);
         if (cmd == "subscribe") return on_app_subscribe(j, s);
         if (cmd == "publish")   return on_app_publish(std::move(j), s);
         if (cmd == "delete")    return on_app_del_post(std::move(j), s);
      } else {
         if (cmd == "login")     return on_app_login(j, s);
         if (cmd == "register")  return on_app_register(j, s);
      }
   } catch (std::exception const& e) {
      log(loglevel::debug, "worker::on_app: {0}.", e.what());
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


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

worker::worker(worker_cfg cfg)
: cfg {cfg.core}
, ch_cfg {cfg.channel}
, ws_timeouts_ {cfg.timeouts}
, db {cfg.db, ioc}
, acceptor {ioc}
, sserver {cfg.stats, ioc}
, signal_set {ioc, SIGINT, SIGTERM}
{
   auto f = [this](auto const& ec, auto n)
      { on_signal(ec, n); };

   signal_set.async_wait(f);

   init();
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

   assert(is_menu_empty(menu));

   // We have to save the menu and retrieve the menu messages from the
   // database. Only after that we will bind and listen for websocket
   // connections.

   menu = j_menu.at("menus").get<menu_elems_array_type>();
   db.retrieve_posts(0);
}

void worker::create_channels(menu_elems_array_type const& me)
{
   assert(std::empty(channel_hashes));
   assert(std::empty(channels));

   channel_hashes.clear();
   channels.clear();

   // Channels are created only for the second menu element.
   auto const hashes = menu_elems_to_codes(me.at(idx::b));

   // Reserver the exact amount of memory that will be needed. The
   // vector with hashes will also hold the sentinel to make linear
   // searches faster.
   channel_hashes.reserve(std::size(hashes) + 1);
   channels.reserve(std::size(hashes));

   auto f = [&](auto const& code)
      { return to_hash_code(code, me.at(idx::b).depth); };

   std::transform( std::cbegin(hashes)
                 , std::cend(hashes)
                 , std::back_inserter(channel_hashes)
                 , f);

   channels.resize(std::size(channel_hashes));

   std::sort(std::begin(channel_hashes), std::end(channel_hashes));

   // Inserts an element that will be used as sentinel.
   channel_hashes.push_back(0);

   log( loglevel::info
      , "Number of channels created: {0}"
      , std::size(channels));
}

void worker::on_db_posts(std::vector<std::string> const& msgs)
{
   // Create the channels only if its empty since this function is
   // also called when the connection to redis is lost and
   // restablished. 
   auto const empty = std::empty(channels);
   if (empty)
      create_channels(menu);

   log( loglevel::info
      , "Number of messages received from the database: {0}"
      , std::size(msgs));

   auto loader = [this](auto const& msg)
      { on_db_channel_post(msg); };

   std::for_each(std::begin(msgs), std::end(msgs), loader);

   if (empty) {
      // We can begin to accept websocket connections. NOTICE: It may
      // be better to use acceptor::is_open to determine if the run
      // functions should be called instead of using empty.
      acceptor.run( *this
                  , cfg.port
                  , cfg.max_listen_connections);

      sserver.run(*this);
   }
}

void worker::on_db_channel_post(std::string const& msg)
{
   // This function has two roles, deal with the delete command and
   // new posts.
   //
   // NOTE: Later we may have to improve how we decide if a message is
   // a post or a command, like delete, specially if the number of
   // commands that are sent from workers to workers increases.

   auto const j = json::parse(msg);
   auto const channel = j.at("to").get<menu_code_type>();

   // The channel above has the form
   //
   //      ____menu_1____    ____menu_2____
   //     |              |  |              |
   //    [[[1, 2, 3, ...]], [[a, b, c, ...]]]
   //
   // We want to convert the second code [a, b, c] to its hash.
   //
   auto const hash =
      to_hash_code( channel.at(idx::b).front()
                  , menu.at(idx::b).depth);

   // Now we perform a binary search of the channel hash code
   // calculated above in the channel_hashes to determine in which
   // channel it shall be published. Notice we exclude the sentinel
   // from the search.
   auto const cend = std::cend(channel_hashes);
   auto const match =
      std::lower_bound( std::cbegin(channel_hashes)
                      , std::prev(cend)
                      , hash);

   // TODO: Will the last condition be evaluated if match is end? 
   if (match == std::prev(cend) || *match > hash) {
      // This happens if the subscription to the posts channel happens
      // before we receive the menu and generate the channels.  That
      // is why it is important to update the last message id only
      // after this check.
      //
      // We could in principle insert the post, but I am afraid this
      // could result in duplicated posts after we receive those from
      // the database.
      log(loglevel::debug, "Channel could not be found: {0}", hash);
      return;
   }

   // The channel has been found, we can now calculate the offset in
   // the channels array.
   auto const i = std::distance(std::cbegin(channel_hashes), match);

   if (j.contains("cmd")) {
      auto const post_id = j.at("id").get<int>();
      auto const from = j.at("from").get<std::string>();
      auto const r = channels[i].remove_post(post_id, from);
      if (!r) 
         log(loglevel::notice, "Failed to remove post: {0}.", post_id);

      return;
   }

   auto const item = j.get<post>();

   if (item.id > last_post_id)
      last_post_id = item.id;

   channels[i].broadcast(item, menu.at(idx::a).depth);
   none_channel.broadcast(item, menu.at(idx::a).depth);
}

void worker::on_db_user_id(std::string const& id)
{
   assert(!std::empty(reg_queue));

   while (!std::empty(reg_queue)) {
      if (auto session = reg_queue.front().session.lock()) {
         reg_queue.front().pwd = pwdgen(cfg.pwd_size);

         // A hashed version of the password is stored in the
         // database.
         auto const digest = make_hex_digest(reg_queue.front().pwd);
         db.register_user(id, digest);
         session->set_id(id);
         return;
      }

      // The user is not online anymore. We try with the next element
      // in the queue, if all of them are also not online anymore the
      // requested id is lost. Very unlikely to happen since the
      // communication with redis is very fast.
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
               , "Login failed for {1}:{2}."
               , s->get_id()
               , login_queue.front().pwd);

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
      session->send(resp.dump(), false);
   } else {
      // The user is not online anymore. The requested id is lost.
   }

   reg_queue.pop();
   if (!std::empty(reg_queue))
      db.request_user_id();
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

// Keep the switch cases in the same sequence declared in the enum to
// improve performance.
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

      case redis::request::channel_post:
         assert(std::size(data) == 1);
         on_db_channel_post(data.back());
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
      // this. It may be a hint that the ssystem is overloaded.

      log(loglevel::notice, "Sending publish_ack to the database.");

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

   auto const pwd = j.at("password").get<std::string>();

   // We do not have to serialize the calls to retrieve_user_data
   // since this will be done by the db object.
   login_queue.push({s, pwd});
   db.retrieve_user_data(s->get_id());

   return ev_res::login_ok;
}

ev_res
worker::on_app_register( json const& j
                       , std::shared_ptr<worker_session> s)
{
   auto const empty = std::empty(reg_queue);
   reg_queue.push({s, {}});
   if (empty)
      db.request_user_id();

   return ev_res::register_ok;
}

ev_res
worker::on_app_subscribe( json const& j
                        , std::shared_ptr<worker_session> s)
{
   auto const codes = j.at("channels").get<menu_code_type2>();

   // The codes above have the form
   //
   //      ____menu_1_____    ____menu_2__ 
   //     |               |  |            |
   //    [[a, b, c, d, ...], [e, f, g, ...]]
   //

   auto const b0 =
      std::is_sorted( std::cbegin(codes.at(idx::a))
                    , std::cend(codes.at(idx::a)));

   auto const b1 =
      std::is_sorted( std::cbegin(codes.at(idx::b))
                    , std::cend(codes.at(idx::b)));

   if (!b0 || !b1) {
      json resp;
      resp["cmd"] = "subscribe_ack";
      resp["result"] = "fail";
      s->send(resp.dump(), false);
      return ev_res::subscribe_fail;
   }

   auto const any_of_features =
      j.at("any_of_features").get<std::uint64_t>();

   auto const app_last_post_id = j.at("last_post_id").get<int>();

   s->set_any_of_features(any_of_features);

   s->set_filter(codes.at(idx::a));

   auto psession = s->get_proxy_session(true);

   std::vector<post> items;

   // If the second channels are empty, the app wants posts from all
   // channels, otherwise we have to traverse the individual channels.
   if (std::empty(codes.at(idx::b))) {
      none_channel.add_member(psession, ch_cfg.cleanup_rate);
      none_channel.get_posts( app_last_post_id
                            , std::back_inserter(items)
                            , cfg.max_posts_on_sub);
   } else {
      // We will use the following algorithm
      // 1. Search the first channel the user subscribed to.
      // 2. Begin a linear search skipping the channels the user did
      // not subscribed to.
      //
      // NOTICE: Remember to skip the sentinel.
      auto const cend = std::cend(channel_hashes);
      auto match =
         std::lower_bound( std::cbegin(channel_hashes)
                         , std::prev(cend)
                         , codes.at(idx::b).front());

      auto invalid_count = 0;
      if (match == std::prev(cend) || *match > codes.at(idx::b).front()) {
         invalid_count = 1;
      } else {
         auto f = [&, this](auto const& code)
         {
            // Sets the sentinel
            channel_hashes.back() = code;

            // Linear search with sentinel.
            while (*match != code)
               ++match;

            if (match == std::prev(cend)) {
               ++invalid_count;
               return;
            }

            auto const i =
               std::distance(std::cbegin(channel_hashes), match);

            channels[i].add_member(psession, ch_cfg.cleanup_rate);

            channels[i].get_posts( app_last_post_id
                                 , std::back_inserter(items)
                                 , cfg.max_posts_on_sub);
         };

         // There is a limit on how many channels the app is allowed
         // to subscribe to.
         auto const d = std::min( ssize(codes.at(idx::b))
                                , ch_cfg.max_sub);

         std::for_each( std::cbegin(codes.at(idx::b))
                      , std::cbegin(codes.at(idx::b)) + d
                      , f);
      }

      // When there are too many posts in the database, the operation
      // below may become too expensive to and we may want to avoid
      // it. We may want to impose a limit on how big the items array
      // may get.
      if (ssize(items) > cfg.max_posts_on_sub) {
         std::nth_element( std::begin(items)
                         , std::begin(items) + cfg.max_posts_on_sub
                         , std::end(items));

         items.erase( std::begin(items) + cfg.max_posts_on_sub
                    , std::end(items));
      }

      if (invalid_count != 0) {
         log( loglevel::debug
            , "worker::on_app_subscribe: Invalid channels {0}."
            , invalid_count);
      }
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
worker::on_app_filenames(json j, std::shared_ptr<worker_session> s)
{
   //auto const n = j.at("quantity").get<int>();
   auto const n = 5;

   auto f = [this, n]()
      { return pwdgen(n); };

   std::vector<std::string> names;
   std::generate_n(std::back_inserter(names), n, f);

   json resp;
   resp["cmd"] = "filenames_ack";
   resp["result"] = "ok";
   resp["names"] = names;
   s->send(resp.dump(), true);

   return ev_res::delete_ok;
}

ev_res
worker::on_app_publish(json j, std::shared_ptr<worker_session> s)
{
   auto items = j.at("items").get<std::vector<post>>();

   // Before we request a new pub id, we have to check that the post
   // is valid and will not be refused later. This prevents the pub
   // items from being incremented on an invalid message. May be
   // overkill since we do not expect the app to contain so severe
   // bugs but we have care for server exploitation.

   // The channel code has the form [[[1, 2]], [[2, 3, 4]]]
   // where each array in the outermost array refers to one menu.
   auto const hash =
      to_hash_code( items.front().to.at(idx::b).front()
                  , menu.at(idx::b).depth);

   auto const cend = std::cend(channel_hashes);
   auto const match =
      std::lower_bound( std::cbegin(channel_hashes)
                      , std::prev(cend)
                      , hash);

   if (match == std::prev(cend) || std::size(items) != 1) {
      // This is a non-existing channel. Perhaps the json command was
      // sent with the wrong information signaling a logic error in
      // the app. Sending a fail ack back to the app is useful to
      // debug it. See redis::request::unsolicited_publish.
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
         , "Sending user messages back to the database: {0}"
         , user_id);

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
   log(loglevel::notice, "Shutdown has been requested.");

   log( loglevel::notice
      , "Number of sessions that will be closed: {0}"
      , std::size(sessions));

   acceptor.shutdown();
   sserver.shutdown();

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
         if (cmd == "message")
            return on_app_chat_msg(std::move(j), s);
         if (cmd == "subscribe")
            return on_app_subscribe(j, s);
         if (cmd == "publish")
            return on_app_publish(std::move(j), s);
         if (cmd == "delete")
            return on_app_del_post(std::move(j), s);
         if (cmd == "filenames")
            return on_app_filenames(std::move(j), s);
      } else {
         if (cmd == "login")
            return on_app_login(j, s);
         if (cmd == "register")
            return on_app_register(j, s);
      }
   } catch (std::exception const& e) {
      log(loglevel::debug, "worker::on_app: {0}.", e.what());
   }

   return ev_res::unknown;
}

void worker::on_signal(boost::system::error_code const& ec, int n)
{
   if (ec) {
      if (ec == net::error::operation_aborted) {
         // No signal occurred, the handler was canceled. We just
         // leave.
         return;
      }

      log( loglevel::crit
         , "listener::on_signal: Unhandled error '{0}'"
         , ec.message());

      return;
   }

   log( loglevel::notice
      , "Signal {0} has been captured. "
        " Stopping listening for new connections"
      , n);

   shutdown();
}

void worker::run()
{
   ioc.run();
}

worker_stats worker::get_stats() const noexcept
{
   worker_stats wstats {};

   wstats.number_of_sessions = ws_stats_.number_of_sessions;
   wstats.worker_post_queue_size = ssize(post_queue);
   wstats.worker_reg_queue_size = ssize(reg_queue);
   wstats.worker_login_queue_size = ssize(login_queue);
   wstats.db_post_queue_size = db.get_post_queue_size();
   wstats.db_chat_queue_size = db.get_chat_queue_size();

   return wstats;
}

}


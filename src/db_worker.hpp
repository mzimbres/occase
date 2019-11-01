#pragma once

#include <queue>
#include <array>
#include <thread>
#include <vector>
#include <memory>
#include <string>
#include <numeric>
#include <iterator>
#include <algorithm>
#include <functional>
#include <unordered_map>

#include <boost/asio.hpp>
#include <boost/asio/signal_set.hpp>

#include <fmt/format.h>

#include "net.hpp"
#include "menu.hpp"
#include "post.hpp"
#include "utils.hpp"
#include "redis.hpp"
#include "logger.hpp"
#include "crypto.hpp"
#include "channel.hpp"
#include "db_session.hpp"
#include "acceptor_mgr.hpp"
#include "db_plain_session.hpp"
#include "db_ssl_session.hpp"

namespace rt
{

struct core_cfg {
   // The maximum number of channels that is allowed to be sent to the
   // user on subscribe.
   int max_posts_on_sub; 

   // The size of the password sent to the app when it registers.
   int pwd_size; 

   // Websocket port.
   unsigned short db_port;

   // The size of the tcp backlog.
   int max_listen_connections;

   // The key used to generate authenticated filenames that will be
   // user in the image server.
   std::string mms_key;

   // DB host.
   std::string db_host;

   // The host where images are served.
   std::string mms_host;

   // The maximum duration time for a http session.
   int http_session_timeout {30};

   // SSL shutdown timeout.
   int ssl_shutdown_timeout {30};

   // The server name.
   std::string server_name {"occase-db"};
};

struct db_worker_cfg {
   redis::config db;
   ws_timeouts timeouts;
   core_cfg core;
   channel_cfg channel;
};

struct ws_stats {
   int number_of_sessions {0};
};

// We have to store publish items in this queue while we wait for
// the pub id that were requested from redis.
template <class WebSocketSession>
struct post_queue_item {
   std::weak_ptr<WebSocketSession> session;
   post item;
};

template <class WebSocketSession>
struct pwd_queue_item {
   std::weak_ptr<WebSocketSession> session;
   std::string pwd;
};

template <class AdmSession>
class db_worker {
private:
   using db_session_type = typename AdmSession::db_session_type;

   net::io_context ioc {1};
   ssl::context& ctx;

   core_cfg const cfg;

   // There are hundreds of thousends of channels typically, so we
   // store the configuration only once here.
   channel_cfg const ch_cfg;

   ws_timeouts const ws_timeouts_;
   ws_stats ws_stats_;

   // Maps a user id in to a websocket session.
   std::unordered_map< std::string
                     , std::weak_ptr<db_session_type>
                     > sessions;

   // The channels are stored in a vector and the channel hash codes
   // separately in another vector. The last element in the
   // tmp_channels is used as sentinel to perform fast linear
   // searches.
   channels_type tmp_channels;
   std::vector<channel<db_session_type>> channel_objs;

   // Apps that do not register for any product channels (the seconds
   // item in the menu) will receive posts from any channel. This is
   // the channel that will store the web socket channels for this
   // case.
   channel<db_session_type> none_channel;

   // Facade to redis.
   redis::facade db;

   // Queue of user posts waiting for an id that has been requested
   // from redis.
   std::queue<post_queue_item<db_session_type>> post_queue;

   // Queue with sessions waiting for a user id that are retrieved
   // from redis.
   std::queue<pwd_queue_item<db_session_type>> reg_queue;

   // Queue of users waiting to be checked for login.
   std::queue<pwd_queue_item<db_session_type>> login_queue;

   // The last post id that this channel has received from reidis
   // pubsub menu channel.
   int last_post_id = 0;

   // Generates passwords that are sent to the app.
   pwd_gen pwdgen;

   // Accepts both http connections used by the administrative api or
   // websocket connection used by the database.
   acceptor_mgr<AdmSession> acceptor_;

   // Signal handler.
   net::signal_set signal_set;

   void init()
   {
      auto handler = [this](auto data, auto const& req)
         { on_db_event(std::move(data), req); };

      db.set_on_msg_handler(handler);
      db.run();
   }

   void create_channels()
   {
      assert(std::empty(channel_objs));

      // Reserve the exact amount of memory that will be needed. The
      // vector with the channels will also hold the sentinel to make
      // linear searches faster.
      channel_objs.resize(std::size(tmp_channels));

      // Inserts an element that will be used as sentinel on linear
      // searches.
      tmp_channels.push_back(0);

      log( loglevel::info
         , "Number of channels created: {0}"
         , std::size(channel_objs));
   }

   ev_res on_app_login(json const& j, std::shared_ptr<db_session_type> s)
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

   ev_res on_app_register(json const& j, std::shared_ptr<db_session_type> s)
   {
      auto const empty = std::empty(reg_queue);
      reg_queue.push({s, {}});
      if (empty)
         db.request_user_id();

      return ev_res::register_ok;
   }

   ev_res on_app_subscribe(json const& j, std::shared_ptr<db_session_type> s)
   {
      auto const channels = j.at("channels").get<channels_type>();
      auto const filters = j.at("filters").get<channels_type>();

      auto const b0 =
         std::is_sorted(std::cbegin(channels), std::cend(channels));

      auto const b1 =
         std::is_sorted(std::cbegin(filters), std::cend(filters));

      if (!b0 || !b1) {
         json resp;
         resp["cmd"] = "subscribe_ack";
         resp["result"] = "fail";
         s->send(resp.dump(), false);
         return ev_res::subscribe_fail;
      }

      auto const any_of_features = j.at("any_of_features").get<code_type>();
      auto const app_last_post_id = j.at("last_post_id").get<int>();

      s->set_any_of_features(any_of_features);
      s->set_filter(filters);
      auto psession = s->get_proxy_session(true);

      std::vector<post> items;

      // If the second channels are empty, the app wants posts from all
      // channels, otherwise we have to traverse the individual channels.
      if (std::empty(channels)) {
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
         auto const cend = std::cend(tmp_channels);
         auto match =
            std::lower_bound( std::cbegin(tmp_channels)
                            , std::prev(cend)
                            , channels.front());

         auto invalid_count = 0;
         if (match == std::prev(cend) || *match > channels.front()) {
            invalid_count = 1;
         } else {
            auto f = [&, this](auto const& code)
            {
               // If the number of posts is already big enough we
               // return immediately. Ideally we would stop traversion
               // the channels, but this is not possible inside the
               // for_each, would have to write a loop.
               if (ssize(items) >= cfg.max_posts_on_sub)
                  return;

               // Sets the sentinel
               tmp_channels.back() = code;

               // Linear search with sentinel.
               while (*match != code)
                  ++match;

               if (match == std::prev(cend)) {
                  ++invalid_count;
                  return;
               }

               auto const i =
                  std::distance(std::cbegin(tmp_channels), match);

               channel_objs[i].add_member(psession, ch_cfg.cleanup_rate);

               auto const n = cfg.max_posts_on_sub - ssize(items);
               assert(n >= 0);

               channel_objs[i].get_posts( app_last_post_id
                                        , std::back_inserter(items)
                                        , n);
            };

            // There is a limit on how many channels the app is allowed
            // to subscribe to.
            auto const d = std::min(ssize(channels), ch_cfg.max_sub);

            std::for_each( std::cbegin(channels)
                         , std::cbegin(channels) + d
                         , f);
         }

         assert(ssize(items) <= cfg.max_posts_on_sub);

         std::sort(std::begin(items), std::end(items));

         if (invalid_count != 0) {
            log( loglevel::debug
               , "db_worker::on_app_subscribe: Invalid channels {0}."
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

   ev_res on_app_chat_msg(json j, std::shared_ptr<db_session_type> s)
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

   ev_res on_app_presence(json j, std::shared_ptr<db_session_type> s)
   {
      // Consider searching the sessions map if the user in this worker
      // and send him his message directly to avoid overloading the redis
      // server. This would be a big optimization in the case of small
      // number of workers.

      j["from"] = s->get_id();
      auto msg = j.dump();
      std::initializer_list<std::string> param = {msg};
      auto const to = j.at("to").get<std::string>();
      db.send_presence(to, msg);

      return ev_res::presence_ok;
   }

   ev_res on_app_publish(json j, std::shared_ptr<db_session_type> s)
   {
      auto items = j.at("items").get<std::vector<post>>();

      if (std::empty(items)) {
         json resp;
         resp["cmd"] = "publish_ack";
         resp["result"] = "fail";
         s->send(resp.dump(), false);
         return ev_res::publish_fail;
      }

      log( loglevel::debug
         , "New post to channel {0}, from user {1}"
         , items.front().to
         , items.front().from);

      // Before we request a new pub id, we have to check that the post
      // is valid and will not be refused later. This prevents the pub
      // items from being incremented on an invalid message. May be
      // overkill since we do not expect the app to contain so severe
      // bugs but we have care for server exploitation.
      auto const cend = std::cend(tmp_channels);
      auto const match =
         std::lower_bound( std::cbegin(tmp_channels)
                         , std::prev(cend)
                         , items.front().to);

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


   ev_res on_app_del_post(json j, std::shared_ptr<db_session_type> s)
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

   ev_res on_app_filenames(json j, std::shared_ptr<db_session_type> s)
   {
      auto const n = sz::mms_filename_min_size;
      auto f = [this, n]()
      {
         auto const filename = pwdgen(n);
         auto const digest = make_hex_digest(filename, cfg.mms_key);
         return digest + "/" + filename;
      };

      std::vector<std::string> names;
      std::generate_n(std::back_inserter(names), n, f);

      json resp;
      resp["cmd"] = "filenames_ack";
      resp["result"] = "ok";
      resp["names"] = names;
      s->send(resp.dump(), true);

      return ev_res::delete_ok;
   }

   // Handlers for events we receive from the database.
   void on_db_chat_msg( std::string const& user_id
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

   void on_db_menu(std::vector<std::string> const& data) noexcept
   {
      try {
         if (std::size(data) != 1) {
            log( loglevel::emerg
               , "Menu received from the database is empty. Exiting ...");

            shutdown();
            return;
         }

         if (std::empty(data.back())) {
            log( loglevel::emerg
               , "Menu received from the database is empty. Exiting ...");

            shutdown();
            return;
         }

         auto const j = json::parse(data.back());

         // We have to save the menu and retrieve the menu messages from the
         // database. Only after that we will bind and listen for websocket
         // connections.
         tmp_channels = j.at("channels").get<channels_type>();
         std::sort(std::begin(tmp_channels), std::end(tmp_channels));
         db.retrieve_posts(0);

         log( loglevel::info
            , "Success retrieving channels from redis.");

      } catch (...) {
         log( loglevel::emerg
            , "Unrecoverable error in: on_db_menu");
         shutdown();
      }
   }

   void on_db_channel_post(std::string const& msg)
   {
      // This function has two roles, deal with the delete command and
      // new posts.
      //
      // NOTE: Later we may have to improve how we decide if a message is
      // a post or a command, like delete, specially if the number of
      // commands that are sent from workers to workers increases.

      auto const j = json::parse(msg);
      auto const to = j.at("to").get<code_type>();

      // Now we perform a binary search of the channel hash code
      // calculated above in the tmp_channels to determine in which
      // channel it shall be published. Notice we exclude the sentinel
      // from the search.
      auto const cend = std::cend(tmp_channels);
      auto const match =
         std::lower_bound( std::cbegin(tmp_channels)
                         , std::prev(cend)
                         , to);

      if (match == std::prev(cend)) {
         // This happens if the subscription to the posts channel happens
         // before we receive the menu and generate the channels.  That
         // is why it is important to update the last message id only
         // after this check.
         //
         // We could in principle insert the post, but I am afraid this
         // could result in duplicated posts after we receive those from
         // the database.
         log(loglevel::debug, "Channel could not be found: {0}", to);
         return;
      }

      if (*match > to) {
         log(loglevel::debug, "Channel could not be found: {0}", to);
         return;
      }

      // The channel has been found, we can now calculate the offset in
      // the channels array.
      auto const i = std::distance(std::cbegin(tmp_channels), match);

      if (j.contains("cmd")) {
         auto const post_id = j.at("id").get<int>();
         auto const from = j.at("from").get<std::string>();
         auto const r1 = channel_objs[i].remove_post(post_id, from);
         auto const r2 = none_channel.remove_post(post_id, from);
         if (!r1 || !r2) 
            log( loglevel::notice
               , "Failed to remove post {0} from channel {1}. User {2}"
               , post_id
               , to
               , from);

         return;
      }

      // Do not call this before we know it is not a delete command.
      // Parsing will throw and error.
      auto const item = j.get<post>();

      if (item.id > last_post_id)
         last_post_id = item.id;

      channel_objs[i].broadcast(item);
      none_channel.broadcast(item);
   }

   void on_db_presence(std::string const& user_id, std::string msg)
   {
      auto const match = sessions.find(user_id);
      if (match == std::end(sessions)) {
         // If the user went offline, we should not be receiving this
         // message. However there may be a timespan where this can
         // happen wo I will simply log.
         log(loglevel::warning, "Receiving presence after unsubscribe.");
         return;
      }

      if (auto s = match->second.lock()) {
         s->send(std::move(msg), false);
         return;
      }
      
      // The user went offline but the session was not removed from
      // the map. This is perhaps not a bug but undesirable as we
      // do not have garbage collector for expired sessions.
      assert(false);
   }

   void on_db_posts(std::vector<std::string> const& msgs)
   {
      // Create the channels only if its empty since this function is
      // also called when the connection to redis is lost and
      // restablished. 
      auto const empty = std::empty(channel_objs);
      if (empty)
         create_channels();

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
         acceptor_.run( *this
                      , ctx
                      , cfg.db_port
                      , cfg.max_listen_connections);
      }
   }

   // Keep the switch cases in the same sequence declared in the enum to
   // improve performance.
   void on_db_event( std::vector<std::string> data
                   , redis::req_item const& req)
   {
      switch (req.req)
      {
         case redis::request::menu:
            on_db_menu(data);
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

         case redis::request::presence:
            assert(std::size(data) == 1);
            on_db_presence(req.user_id, data.front());
            break;

         default:
            break;
      }
   }

   void on_db_post_id(std::string const& post_id_str)
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

   void on_db_publish()
   {
      // We do not need this function at the moment.
   }

   void on_db_menu_connect()
   {
      // There are two situations we have to distinguish here.
      //
      // 1. The server has started and still does not have the menu, we
      //    have to retrieve it.
      //
      // 2. The connection to the database (redis) has been lost and
      //    restablished. In this case we have to retrieve all the
      //    messages that may have arrived while we were offline.

      if (std::empty(tmp_channels)) {
         db.retrieve_menu();
      } else {
         db.retrieve_posts(1 + last_post_id);
      }
   }

   void on_db_user_id(std::string const& id)
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

   void on_db_register()
   {
      assert(!std::empty(reg_queue));
      if (auto session = reg_queue.front().session.lock()) {
         auto const& id = session->get_id();
         db.on_user_online(id);
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

   void on_db_user_data(std::vector<std::string> const& data) noexcept
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

               db.on_user_online(s->get_id());
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

   void on_signal(boost::system::error_code const& ec, int n)
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
         , n);

      shutdown_impl();
   }

   // WARNING: Do not call this function from the signal handler.
   void shutdown()
   {
      shutdown_impl();

      boost::system::error_code ec;
      signal_set.cancel(ec);
      if (ec) {
         log( loglevel::info
            , "db_worker::shutdown: {0}"
            , ec.message());
      }
   }

   void shutdown_impl()
   {
      log(loglevel::notice, "Shutdown has been requested.");

      log( loglevel::notice
         , "Number of sessions that will be closed: {0}"
         , std::size(sessions));

      acceptor_.shutdown();

      auto f = [](auto o)
      {
         if (auto s = o.second.lock())
            s->shutdown();
      };

      std::for_each(std::begin(sessions), std::end(sessions), f);

      db.disconnect();
   }

public:
   db_worker(db_worker_cfg cfg, ssl::context& c)
   : ctx {c}
   , cfg {cfg.core}
   , ch_cfg {cfg.channel}
   , ws_timeouts_ {cfg.timeouts}
   , db {cfg.db, ioc}
   , acceptor_ {ioc}
   , signal_set {ioc, SIGINT, SIGTERM}
   {
      auto f = [this](auto const& ec, auto n)
         { on_signal(ec, n); };

      signal_set.async_wait(f);

      init();
   }

   void on_session_dtor( std::string const& user_id
                       , std::vector<std::string> msgs)
   {
      auto const match = sessions.find(user_id);
      if (match == std::end(sessions))
         return;

      sessions.erase(match);

      db.on_user_offline(user_id);

      if (!std::empty(msgs)) {
         log( loglevel::debug
            , "Sending user messages back to the database: {0}"
            , user_id);

         db.store_chat_msg( std::move(user_id)
                          , std::make_move_iterator(std::begin(msgs))
                          , std::make_move_iterator(std::end(msgs)));
      }
   }

   ev_res on_app( std::shared_ptr<db_session_type> s
                , std::string msg) noexcept
   {
      try {
         auto j = json::parse(msg);
         auto const cmd = j.at("cmd").get<std::string>();

         if (s->is_logged_in()) {
            if (cmd == "presence")
               return on_app_presence(std::move(j), s);
            if (cmd == "message")
               return on_app_chat_msg(std::move(j), s);
            if (cmd == "subscribe")
               return on_app_subscribe(j, s);
            if (cmd == "filenames")
               return on_app_filenames(std::move(j), s);
            if (cmd == "publish")
               return on_app_publish(std::move(j), s);
            if (cmd == "delete")
               return on_app_del_post(std::move(j), s);
         } else {
            if (cmd == "login")
               return on_app_login(j, s);
            if (cmd == "register")
               return on_app_register(j, s);
         }
      } catch (std::exception const& e) {
         log(loglevel::debug, "db_worker::on_app: {0}.", e.what());
      }

      return ev_res::unknown;
   }

   auto const& get_timeouts() const noexcept
      { return ws_timeouts_;}
   auto& get_ws_stats() noexcept
      { return ws_stats_;}
   auto const& get_ws_stats() const noexcept
      { return ws_stats_; }

   worker_stats get_stats() const noexcept
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

   std::vector<post> get_posts(int id) const noexcept
   {
      try {
         std::vector<post> posts;
         none_channel.get_posts( id
                               , std::back_inserter(posts)
                               , cfg.max_posts_on_sub);
         return posts;
      } catch (std::exception const& e) {
         log(loglevel::info, "get_posts: {}", e.what());
      }

      return {};
   }

   auto& get_ioc() const noexcept
      { return ioc; }

   void run()
   {
      ioc.run();
   }

   auto const& get_cfg() const noexcept
   {
      return cfg;
   }

   void delete_post( int post_id
                   , std::string const& from
                   , code_type to)
   {
      // See comment is on_app_del_post.

      json j;
      j["cmd"] = "delete";
      j["from"] = from;
      j["id"] = post_id;
      j["to"] = to;
      db.remove_post(post_id, j.dump());
   }
};

}


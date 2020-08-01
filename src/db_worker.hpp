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
#include "post.hpp"
#include "utils.hpp"
#include "redis.hpp"
#include "logger.hpp"
#include "crypto.hpp"
#include "channel.hpp"
#include "db_session.hpp"
#include "acceptor_mgr.hpp"
#include "db_ssl_session.hpp"
#include "db_plain_session.hpp"

namespace occase
{

struct core_cfg {
   // The maximum number of posts that are allowed to be sent to the
   // user on subscribe.
   int max_posts_on_sub; 

   // The size of the password sent to the app when it registers.
   int pwd_size; 

   // The port on which the database listens.
   unsigned short db_port;

   // TCP backlog size.
   int max_listen_connections;

   // The key used to generate authenticated filenames that will be
   // user in the image server.
   std::string mms_key;

   // The host name of this db. This is used by the adm interface when the html
   // pages are generated.
   std::string db_host;

   // The host where images are served and posted. It must have the
   // form http://occase.de/
   std::string mms_host;

   // The maximum duration time for the adm http session in seconds.
   int http_session_timeout {30};

   // SSL shutdown timeout in seconds.
   int ssl_shutdown_timeout {30};

   // Server name.
   std::string server_name {"occase-db"};

   // The password required to access the adm html pages.
   std::string adm_pwd;

   // The number of posts users are allowed to post until the deadline below is
   // reached. This value is also configurable via adm api.
   int allowed_posts = 0;

   // The deadline for the number of posts defined above.
   long int post_interval {100000};

   auto get_post_interval() const noexcept
      { return std::chrono::seconds {post_interval}; }
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
   std::string token;
};

template <class AdmSession>
class db_worker {
private:
   using db_session_type = typename AdmSession::db_session_type;

   net::io_context ioc_ {BOOST_ASIO_CONCURRENCY_HINT_UNSAFE};
   ssl::context& ctx_;
   core_cfg const core_cfg_;

   channel_cfg const channel_cfg_;

   ws_timeouts const ws_timeouts_;
   ws_stats ws_stats_;

   // Maps a user id in to a websocket session.
   std::unordered_map< std::string
                     , std::weak_ptr<db_session_type>
                     > sessions_;

   channel<db_session_type> root_channel_;

   // Facade to redis.
   redis db_;

   // Queue of user posts waiting for an id that has been requested
   // from redis.
   std::queue<post_queue_item<db_session_type>> post_queue_;

   // Queue with sessions waiting for a user id that are retrieved
   // from redis.
   std::queue<pwd_queue_item<db_session_type>> reg_queue_;

   // Queue of users waiting to be checked for login.
   std::queue<pwd_queue_item<db_session_type>> login_queue_;

   // The last post id that has beed received from reidis pubsub channel.
   int last_post_id_ = -1;

   // Generates passwords that are sent to the app.
   pwd_gen pwdgen_;

   // Accepts both http connections used by the administrative api or
   // websocket connection used by the database.
   acceptor_mgr<AdmSession> acceptor_;

   // Signal handler.
   net::signal_set signal_set_;

   void init()
   {
      auto handler = [this](auto data, auto const& req)
         { on_db_event(std::move(data), req); };

      db_.set_event_handler(handler);
      db_.run();
   }

   ev_res on_app_login(json const& j, std::shared_ptr<db_session_type> s)
   {
      auto const user = j.at("user").get<std::string>();
      s->set_id(user);

      auto const pwd = j.at("password").get<std::string>();

      std::string token;
      auto const match = j.find("token");
      if (match != std::cend(j))
         token = *match;

      // We do not have to serialize the calls to retrieve_user_data
      // since this will be done by the db_ object.
      login_queue_.push({s, pwd, token});
      db_.retrieve_user_data(s->get_id());

      return ev_res::login_ok;
   }

   ev_res on_app_register(json const& j, std::shared_ptr<db_session_type> s)
   {
      // If the app sent us the token we have to make it available to
      // occase-notify. For that we have to store it with the password and
      // publish in the tokens channel toguether when we send the
      // register_ack.

      std::string token;
      auto const match = j.find("token");
      if (match != std::cend(j))
         token = *match;

      auto const empty = std::empty(reg_queue_);
      reg_queue_.push({s, {}, token});
      if (empty)
         db_.request_user_id();

      return ev_res::register_ok;
   }

   ev_res on_app_subscribe(json const& j, std::shared_ptr<db_session_type> s)
   {
      std::vector<post> items;

      auto pred = [s](auto const& p)
         { return !s->ignore(p); };

      root_channel_.add_member(s, channel_cfg_.cleanup_rate);

      root_channel_.get_posts(
	 -1,
	 std::back_inserter(items),
	 core_cfg_.max_posts_on_sub,
	 pred);

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

   ev_res
   on_app_chat_msg( json j
                  , std::shared_ptr<db_session_type> s)
   {
      j["from"] = s->get_id();

      // If the user is online in this node we can send him a message
      // directly.  This is important to reduce the amount of data in
      // redis, occase-notify and to reduce the communication latency.
      auto const to = j.at("to").get<std::string>();
      auto const match = sessions_.find(to);
      if (match == std::end(sessions_)) {
         // The peer is either offline or not in this node. We have to
         // store the message in the database (redis).
         auto param = {j.dump()};
         db_.store_chat_msg( to
                           , std::make_move_iterator(std::begin(param))
                           , std::make_move_iterator(std::end(param)));
      } else {
         // The peer is online and in this node, we can send him the
         // message directly.
         if (auto ss = match->second.lock())
            ss->send(j.dump(), true);
      }

      // No need to store the ack in the database as the user will resend
      // the message if the connection breaks and has to be restablished. 
      auto const post_id = j.at("post_id").get<int>();
      auto const message_id = j.at("id").get<int>();
      json ack;
      ack["cmd"] = "message";
      ack["from"] = to;
      ack["post_id"] = post_id;
      ack["ack_id"] = message_id;
      ack["type"] = "server_ack";
      ack["result"] = "ok";

      s->send(ack.dump(), false);
      return ev_res::chat_msg_ok;
   }

   ev_res
   on_app_presence( json j
                  , std::shared_ptr<db_session_type> s)
   {
      // See also comments in on_app_chat_msg.

      j["from"] = s->get_id();

      auto const to = j.at("to").get<std::string>();
      auto const match = sessions_.find(to);
      if (match == std::end(sessions_)) {
         db_.send_presence(to, j.dump());
      } else {
         if (auto ss = match->second.lock())
            ss->send(j.dump(), false);
      }

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

      log::write( log::level::debug
                , "New post from user {0}"
                , items.front().from);

      // Before we request a new pub id, we have to check that the post
      // is valid and will not be refused later. This prevents the pub
      // items from being incremented on an invalid message. May be
      // overkill since we do not expect the app to contain so severe
      // bugs but we never thrust the client.
      // TODO: How to check post validity.

      // I do not check if the deadline is valid as it should always be
      // according to the algorithm used.
      if (s->get_remaining_posts() < 1) {
         json resp;
         resp["cmd"] = "publish_ack";
         resp["result"] = "fail";
         s->send(resp.dump(), false);
         return ev_res::publish_fail;
      }

      using namespace std::chrono;

      // It is important not to thrust the *from* field in the json
      // command.
      items.front().from = s->get_id();
      items.front().date =
         duration_cast<seconds>(system_clock::now().time_since_epoch());
      post_queue_.push({s, items.front()});
      db_.request_post_id();
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
      db_.remove_post(post_id, j.dump());

      json resp;
      resp["cmd"] = "delete_ack";
      resp["result"] = "ok";
      s->send(resp.dump(), true);

      return ev_res::delete_ok;
   }

   ev_res on_app_filenames(json j, std::shared_ptr<db_session_type> s)
   {
      // NOTE: Earlier I was sending fail if
      //
      //    if (s->get_remaining_posts() < 1)
      //       ...
      //
      // This is incorrected however as it prevents people from
      // sending paid posts for example if they have used all its free
      // posts.

      // The filenames generated by the lambda below have the form
      // (see also post.hpp)
      //
      //    http://mms.occase.de/x/x/xx/filename-digest
      //
      // The digest part corresponds to the part
      //
      //    /x/x/xx/filename
      //
      auto f = [this]()
      {
         auto const filename = pwdgen_(sz::mms_filename_size);

         auto const path = make_rel_path(filename)
                         + "/"
                         + filename;

         auto const digest = make_hex_digest(path, core_cfg_.mms_key);

         return core_cfg_.mms_host
              + path
              + pwd_gen::sep
              + digest;
      };

      std::vector<std::string> names;
      std::generate_n(std::back_inserter(names),
                      sz::mms_filename_size,
                      f);

      json resp;
      resp["cmd"] = "filenames_ack";
      resp["result"] = "ok";
      resp["names"] = names;
      s->send(resp.dump(), false);

      return ev_res::filenames_ok;
   }

   // Handlers for events we receive from the database.
   void on_db_chat_msg( std::string const& user_id
                      , std::vector<std::string> msgs)
   {
      auto const match = sessions_.find(user_id);
      if (match == std::end(sessions_)) {
         // The user went offline. We have to enqueue the message again.
         // This is difficult to test since the retrieval of messages
         // from the database is pretty fast.
         db_.store_chat_msg( user_id
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

   void on_db_channel_post(std::string const& msg)
   {
      try {
	 // This function has two roles, deal with the delete command and
	 // new posts.
	 //
	 // NOTE: Later we may have to improve how we decide if a message is
	 // a post or a command, like delete, specially if the number of
	 // commands that are sent from workers to workers increases.

	 auto const j = json::parse(msg);

	 if (j.contains("cmd")) {
	    auto const post_id = j.at("id").get<int>();
	    auto const from = j.at("from").get<std::string>();
	    auto const r2 = root_channel_.remove_post(post_id, from);
	    if (!r2) 
	       log::write( log::level::notice
			 , "Failed to remove post {0}. User {1}"
			 , post_id
			 , from);

	    return;
	 }

	 // Do not call this before we know it is not a delete command.  Parsing
	 // will throw and error.
	 auto const item = j.get<post>();

	 if (item.id > last_post_id_)
	    last_post_id_ = item.id;

	 using namespace std::chrono;

	 auto const now =
	    duration_cast<seconds>(system_clock::now().time_since_epoch());

	 auto const post_exp = channel_cfg_.get_post_expiration();

	 auto expired1 = root_channel_.broadcast(item, now, post_exp);

	 std::sort(std::begin(expired1), std::end(expired1));

	 auto new_end = std::unique(std::begin(expired1), std::end(expired1));

	 expired1.erase(new_end, std::end(expired1));

	 // NOTE: When we issue the delete command to the other databases, we are
	 // in fact also sending a delete cmd to ourselves and in this case the
	 // deletions will fail (they have already been removed). This is not bad
	 // since it simplifies the code and there are also tipically not so many
	 // posts in each deletion, it presents no performance problems in any
	 // case.

	 auto f = [this](auto const& p)
	    { delete_post(p.id, p.from); };

	 std::for_each( std::cbegin(expired1)
		      , std::cend(expired1)
		      , f);

	 if (!std::empty(expired1)) {
	    log::write( log::level::info
		      , "Number of expired posts removed: {0}"
		      , std::size(expired1));
	 }
      } catch (std::exception const& e) {
	 log::write( log::level::err
		   , "on_db_channel_post (1): {0}"
		   , e.what());

	 log::write( log::level::err
		   , "on_db_channel_post (2): {0}"
		   , msg);
      }
   }

   void on_db_presence(std::string const& user_id, std::string msg)
   {
      auto const match = sessions_.find(user_id);
      if (match == std::end(sessions_)) {
         // If the user went offline, we should not be receiving this
         // message. However there may be a timespan where this can
         // happen wo I will simply log.
         log::write(log::level::warning, "Receiving presence after unsubscribe.");
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
      // This function is also called when the connection to redis is lost and
      // restablished. 

      log::write( log::level::info
                , "Number of messages received from the database: {0}"
                , std::size(msgs));

      auto loader = [this](auto const& msg)
         { on_db_channel_post(msg); };

      std::for_each(std::begin(msgs), std::end(msgs), loader);

      if (!acceptor_.is_open()) {
	 // We can begin to accept websocket connections.
         acceptor_.run( *this
                      , ctx_
                      , core_cfg_.db_port
                      , core_cfg_.max_listen_connections);
      }
   }

   // Keep the switch cases in the same sequence declared in the enum to
   // improve performance.
   void on_db_event( std::vector<std::string> data
                   , redis::response const& req)
   {
      try {
	 switch (req.req)
	 {
	    case redis::events::user_messages:
	       on_db_chat_msg(req.user_id, std::move(data));
	       break;

	    case redis::events::post:
	       on_db_publish();
	       break;

	    case redis::events::posts:
	       on_db_posts(data);
	       break;

	    case redis::events::post_id:
	       assert(std::size(data) == 1);
	       on_db_post_id(data.back());
	       break;

	    case redis::events::user_id:
	       assert(std::size(data) == 1);
	       on_db_user_id(data.back());
	       break;

	    case redis::events::user_data:
	       on_db_user_data(data);
	       break;

	    case redis::events::register_user:
	       on_db_register();
	       break;

	    case redis::events::post_connect:
	       on_db_post_connect();
	       break;

	    case redis::events::channel_post:
	       assert(std::size(data) == 1);
	       on_db_channel_post(data.back());
	       break;

	    case redis::events::presence:
	       assert(std::size(data) == 1);
	       on_db_presence(req.user_id, data.front());
	       break;

	    default:
	       break;
	 }
      } catch (std::exception const& e) {
         log::write(log::level::crit, "on_db_event: {0}", e.what());
      }
   }

   void on_db_post_id(std::string const& post_id_str)
   {
      auto const post_id = std::stoi(post_id_str);
      post_queue_.front().item.id = post_id;
      json const j_item = post_queue_.front().item;
      db_.post(j_item.dump(), post_id);

      // It is important that the publisher receives this message before
      // any user sends him a user message about the post. He needs a
      // post_id to know to which post the user refers to.
      json ack;
      ack["cmd"] = "publish_ack";
      ack["result"] = "ok";
      ack["id"] = post_queue_.front().item.id;
      ack["date"] = post_queue_.front().item.date.count();
      auto ack_str = ack.dump();

      if (auto s = post_queue_.front().session.lock()) {
         s->send(std::move(ack_str), true);

         db_.update_remaining(
            s->get_id(),
            s->decrease_remaining_posts());

      } else {
         // If we get here the user is not online anymore. This should be a
         // very rare situation since requesting an id takes milliseconds.
         // We can simply store his ack in the database for later retrieval
         // when the user connects again. It shall be difficult to test
         // this. It may be a hint that the ssystem is overloaded.

         log::write(log::level::notice, "Sending publish_ack to the database.");

         auto param = {ack_str};
         db_.store_chat_msg( std::move(post_queue_.front().item.from)
                           , std::make_move_iterator(std::begin(param))
                           , std::make_move_iterator(std::end(param)));
      }

      post_queue_.pop();
   }

   void on_db_publish()
   {
      // We do not need this function at the moment.
   }

   void on_db_post_connect()
   {
      // There are two situations we have to distinguish here.
      //
      // 1. The server has started.
      //
      // 2. The connection to the database (redis) was lost and restablished.
      //    In this case we have to retrieve all posts that may have arrived
      //    while we were offline.

      db_.retrieve_posts(1 + last_post_id_);
   }

   void on_db_user_id(std::string const& id)
   {
      using namespace std::chrono;
      assert(!std::empty(reg_queue_));

      while (!std::empty(reg_queue_)) {
         if (auto s = reg_queue_.front().session.lock()) {
            reg_queue_.front().pwd = pwdgen_(core_cfg_.pwd_size);

            // A hashed version of the password is stored in the
            // database.
            auto const digest = make_hex_digest(reg_queue_.front().pwd);

            auto const now =
               duration_cast<seconds>(system_clock::now()
                  .time_since_epoch());

            auto const deadline = now + core_cfg_.get_post_interval();

            db_.register_user( id
                             , digest
                             , core_cfg_.allowed_posts
                             , deadline);

            s->set_id(id);
            s->set_remaining_posts(core_cfg_.allowed_posts);

            log::write( log::level::info
                      , "New user: {0}. Remaining posts {1}."
                      , id
                      , core_cfg_.allowed_posts);

            return;
         }

         // The user is not online anymore. We try with the next element
         // in the queue, if all of them are also not online anymore the
         // requested id is lost. Very unlikely to happen since the
         // communication with redis is very fast.
         reg_queue_.pop();
      }
   }

   void on_db_register()
   {
      assert(!std::empty(reg_queue_));
      if (auto session = reg_queue_.front().session.lock()) {
         auto const& id = session->get_id();
         db_.on_user_online(id);
         auto const new_user = sessions_.insert({id, session});

         // It would be a bug if this id were already in the map since we
         // have just registered it.
         assert(new_user.second);

         json resp;
         resp["cmd"] = "register_ack";
         resp["result"] = "ok";
         resp["id"] = id;
         resp["password"] = reg_queue_.front().pwd;
         session->send(resp.dump(), false);

         if (!std::empty(reg_queue_.front().token))
            db_.publish_token(
               session->get_id(),
               reg_queue_.front().token);
      } else {
         // The user is not online anymore. The requested id is lost.
      }

      reg_queue_.pop();
      if (!std::empty(reg_queue_))
         db_.request_user_id();
   }

   void on_db_user_data(std::vector<std::string> const& fields) noexcept
   {
      assert(!std::empty(login_queue_));
      assert(std::size(fields) == 4);

      if (auto s = login_queue_.front().session.lock()) {
	 auto const digest = make_hex_digest(login_queue_.front().pwd);
	 if (fields[0] != digest) { // Incorrect pwd?
	    json resp;
	    resp["cmd"] = "login_ack";
	    resp["result"] = "fail";
	    s->send(resp.dump(), false);
	    s->shutdown();

	    log::write( log::level::debug
		      , "Login failed for {0}:{1}."
		      , s->get_id()
		      , login_queue_.front().pwd);

	 } else {
	    auto const ss = sessions_.insert({s->get_id(), s});
	    if (!ss.second) {
	       // There should never be more than one session with
	       // the same id. For now, I will simply override the
	       // old session and disregard any pending messages.
	       // In Principle, if the session is still online, it
	       // should not contain any messages anyway.

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

	    db_.on_user_online(s->get_id());
	    db_.retrieve_messages(s->get_id());

	    using namespace std::chrono;
	    auto const now = duration_cast<seconds>(
	       system_clock::now().time_since_epoch());

	    auto const allowed = std::stoi(fields.at(1));
	    auto remaining = std::stoi(fields.at(2));
	    auto const deadline = seconds {std::stol(fields.at(3))};
	    if (now > deadline) {
	       // Notice that a race can originate here if the adm api has
	       // updated the number of allowed or remaining posts right
	       // after these values have been retrieved from redis. This is
	       // extremely unlikely as there are only a couple of micro
	       // seconds for that to happen.
	       // 
	       // Once the user is logged in we should never again update
	       // his post counters as that can overwrite values set over
	       // the adm api. Reading it before setting is too complicated.
	       db_.update_post_deadline(
		  s->get_id(),
		  allowed,
		  now + core_cfg_.get_post_interval());

	       remaining = allowed;
	    }

	    s->set_remaining_posts(remaining);

	    json resp;
	    resp["cmd"] = "login_ack";
	    resp["result"] = "ok";
	    resp["remaining_posts"] = remaining;
	    s->send(resp.dump(), false);

	    if (!std::empty(login_queue_.front().token))
	       db_.publish_token(
		  s->get_id(),
		  login_queue_.front().token);
	 }
      } else {
	 // The user is not online anymore. The requested id is lost.
	 // Very unlikely to happen since the communication with redis is
	 // very fast.
      }

      login_queue_.pop();
   }

   void on_signal(boost::system::error_code const& ec, int n)
   {
      if (ec) {
         if (ec == net::error::operation_aborted) {
            // No signal occurred, the handler was canceled. We just
            // leave.
            return;
         }

         log::write( log::level::crit
                   , "listener::on_signal: Unhandled error '{0}'"
                   , ec.message());

         return;
      }

      log::write( log::level::notice
                , "Signal {0} has been captured. " 
                , n);

      shutdown_impl();
   }

   // WARNING: Do not call this function from the signal handler.
   void shutdown()
   {
      shutdown_impl();

      boost::system::error_code ec;
      signal_set_.cancel(ec);
      if (ec) {
         log::write( log::level::info
                   , "db_worker::shutdown: {0}"
                   , ec.message());
      }
   }

   void shutdown_impl()
   {
      log::write(log::level::notice, "Shutdown has been requested.");

      log::write( log::level::notice
                , "Number of sessions that will be closed: {0}"
                , std::size(sessions_));

      acceptor_.shutdown();

      auto f = [](auto o)
      {
         if (auto s = o.second.lock())
            s->shutdown();
      };

      std::for_each(std::begin(sessions_), std::end(sessions_), f);

      db_.disconnect();
   }

public:
   db_worker(db_worker_cfg cfg, ssl::context& c)
   : ctx_ {c}
   , core_cfg_ {cfg.core}
   , channel_cfg_ {cfg.channel}
   , ws_timeouts_ {cfg.timeouts}
   , db_ {cfg.db, ioc_}
   , acceptor_ {ioc_}
   , signal_set_ {ioc_, SIGINT, SIGTERM}
   {
      auto f = [this](auto const& ec, auto n)
         { on_signal(ec, n); };

      signal_set_.async_wait(f);

      init();
   }

   void on_session_dtor( std::string const& user_id
                       , std::vector<std::string> msgs)
   {
      auto const match = sessions_.find(user_id);
      if (match == std::end(sessions_))
         return;

      sessions_.erase(match);

      db_.on_user_offline(user_id);

      if (!std::empty(msgs)) {
         log::write( log::level::debug
                   , "Sending user messages back to the database: {0}"
                   , user_id);

         db_.store_chat_msg( std::move(user_id)
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
         log::write(log::level::debug, "db_worker::on_app: {0}.", e.what());
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
      wstats.worker_post_queue_size = ssize(post_queue_);
      wstats.worker_reg_queue_size = ssize(reg_queue_);
      wstats.worker_login_queue_size = ssize(login_queue_);
      wstats.db_post_queue_size = db_.get_post_queue_size();
      wstats.db_chat_queue_size = db_.get_chat_queue_size();

      return wstats;
   }

   template <class UnaryPredicate>
   std::vector<post>
   get_posts(int date, UnaryPredicate pred) const noexcept
   {
      try {
         std::vector<post> posts;

         root_channel_.get_posts(
            date,
            std::back_inserter(posts),
            core_cfg_.max_posts_on_sub,
            pred);

         return posts;
      } catch (std::exception const& e) {
         log::write(log::level::info, "get_posts: {}", e.what());
      }

      return {};
   }

   auto& get_ioc() const noexcept
      { return ioc_; }

   void run()
      { ioc_.run(); }

   auto const& get_cfg() const noexcept
      { return core_cfg_; }

   void delete_post(int post_id, std::string const& from)
   {
      // See comment is on_app_del_post.

      json j;
      j["cmd"] = "delete";
      j["from"] = from;
      j["id"] = post_id;
      db_.remove_post(post_id, j.dump());
   }
};

}


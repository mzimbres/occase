#pragma once

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
#include "acceptor_mgr.hpp"
#include "http_plain_session.hpp"
#include "http_ssl_session.hpp"
#include "ws_session_impl.hpp"
#include "ws_ssl_session.hpp"
#include "ws_plain_session.hpp"

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

   // Value of the header field Access-Control-Allow-Origin
   std::string http_allow_origin {"*"};

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
   channel::config ch;
};

struct ws_stats {
   int number_of_sessions {0};
};

template <class Stream>
struct make_session_type;

template <>
struct make_session_type<beast::tcp_stream> {
  using websocket = ws_plain_session;
  using http = http_plain_session;
};

template <class T>
struct make_session_type<beast::ssl_stream<T>> {
  using websocket = ws_ssl_session;
  using http = http_ssl_session;
};

template <class Stream>
class db_worker {
private:
   using ws_session_type = typename make_session_type<Stream>::websocket;
   using http_session_type = typename make_session_type<Stream>::http;

   net::io_context ioc_ {BOOST_ASIO_CONCURRENCY_HINT_UNSAFE};
   ssl::context& ctx_;
   core_cfg const core_cfg_;

   channel::config const channel_cfg_;

   ws_timeouts const ws_timeouts_;
   ws_stats ws_stats_;

   // Maps a user id in to a websocket session.
   std::unordered_map< std::string
                     , std::weak_ptr<ws_session_type>
                     > sessions_;

   channel root_channel_;

   // Facade to redis.
   redis db_;

   // The last post id that has beed received from reidis pubsub channel.
   date_type last_post_date_received_ {0};

   // Generates passwords that are sent to the app.
   pwd_gen pwdgen_;

   // Accepts both http connections used by the administrative api or
   // websocket connection used by the database.
   acceptor_mgr<http_session_type> acceptor_;

   // Signal handler.
   net::signal_set signal_set_;

   void init()
   {
      auto handler = [this](auto data, auto const& req)
         { on_db_event(std::move(data), req); };

      db_.set_event_handler(handler);
      db_.run();
   }

   auto on_app_login(json const& j, std::shared_ptr<ws_session_type> s)
   {
      auto const user = j.at("user").get<std::string>();
      auto const key = j.at("key").get<std::string>();
      auto const user_hash = make_hex_digest(user, key);

      if (std::empty(user_hash)) {
	 json resp;
	 resp["cmd"] = "login_ack";
	 resp["result"] = "fail";
	 s->send(resp.dump(), false);
	 return ev_res::login_fail;
      }

      s->set_pub_hash(user_hash);

      auto const ss = sessions_.insert({user_hash, s});
      if (!ss.second) {
	 // There should never be more than one session with
	 // the same id. For now, I will simply override the
	 // old session and disregard any pending messages.
	 // In principle, if the session is still online, it
	 // should not contain any messages anyway.

	 // The old session has to be shutdown first
	 if (auto old_ss = ss.first->second.lock()) {
	    old_ss->shutdown();
	    // We have to prevent the cleanup operation when its
	    // destructor is called. That would cause the new
	    // session to be removed from the map again.
	    old_ss->set_pub_hash("");
	 } else {
	    // Awckward, the old session has already expired and
	    // we did not remove it from the map. It should be
	    // fixed.
	 }

	 ss.first->second = s;
      }

      auto const match = j.find("token");
      if (match != std::cend(j))
	 if (!std::empty(*match))
	    db_.publish_token(user_hash, *match);

      db_.on_user_online(user_hash);
      db_.retrieve_messages(user_hash);

      json resp;
      resp["cmd"] = "login_ack";
      resp["result"] = "ok";
      resp["remaining_posts"] = 0;

      s->send(resp.dump(), false);

      return ev_res::login_ok;
   }

   auto on_app_chat_msg(json j, std::shared_ptr<ws_session_type> s)
   {
      j["from"] = s->get_pub_hash();

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
      auto const post_id = j.at("post_id").get<std::string>();
      auto const message_id = j.at("id").get<int>();
      json ack;
      ack["cmd"] = "message";
      ack["from"] = to;
      ack["to"] = s->get_pub_hash();
      ack["post_id"] = post_id;
      ack["ack_id"] = message_id;
      ack["type"] = "server_ack";
      ack["result"] = "ok";

      s->send(ack.dump(), false);
      return ev_res::chat_msg_ok;
   }

   auto on_app_presence(json j, std::shared_ptr<ws_session_type> s)
   {
      // See also comments in on_app_chat_msg.

      j["from"] = s->get_pub_hash();

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

   auto on_app_publish(json j, std::shared_ptr<ws_session_type> s)
   {
      s->send(on_publish(j), true);
      return ev_res::publish_ok;
   }

   // Handlers for events we receive from the database.
   void on_db_chat_msg( std::string const& user_hash
                      , std::vector<std::string> msgs)
   {
      auto const match = sessions_.find(user_hash);
      if (match == std::end(sessions_)) {
         // The user went offline. We have to enqueue the message again.
         // This is difficult to test since the retrieval of messages
         // from the database is pretty fast.
         db_.store_chat_msg( user_hash
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
      using namespace std::chrono;
      try {
	 // This function has two roles, deal with the delete command and
	 // new posts.
	 //
	 // NOTE: Later we may have to improve how we decide if a message is
	 // a post or a command, like delete, specially if the number of
	 // commands that are sent from workers to workers increases.

	 auto const j = json::parse(msg);
         auto const cmd = j.at("cmd").get<std::string>();

	 if (cmd == "visualizations") {
            auto const j = json::parse(msg);
            auto post_ids = j.at("post_ids").get<std::vector<std::string>>();
            std::sort(std::begin(post_ids), std::end(post_ids));
            root_channel_.on_visualizations(post_ids);
	    return;
	 }

	 if (cmd == "click") {
            auto const j = json::parse(msg);
            auto const post_id = j.at("post_id").get<std::string>();
            root_channel_.on_click(post_id);
	    return;
	 }

	 if (cmd == "delete") {
	    auto const post_id = j.at("post_id").get<std::string>();
	    auto const from = j.at("from").get<std::string>();
	    if (!root_channel_.remove_post(post_id, from)) 
	       log::write( log::level::notice
			 , "Failed to remove post {0}. User {1}"
			 , post_id
			 , from);

	    return;
	 }

	 if (cmd == "publish_internal") {
            // Do not call this before we know it is not a delete command.
            // Parsing will throw and error.
            auto const item = j.at("post").get<post>();

            if (item.date > last_post_date_received_)
               last_post_date_received_ = item.date;

            auto const now =
               duration_cast<seconds>(system_clock::now().time_since_epoch());
            auto const post_exp = channel_cfg_.get_post_expiration();
            root_channel_.add_post(item);
            auto const expired = root_channel_.remove_expired_posts(now, post_exp);

            // NOTE: When we issue the delete command to the other databases,
            // we are in fact also sending a delete cmd to ourselves and in
            // this case the deletions will fail (they have already been
            // removed). This is not bad since it simplifies the code and there
            // are also tipically not so many posts in each deletion, it
            // presents no performance problems in any case.

            auto f = [this](auto const& p)
               { /*Perhaps won't implement this.*/ };

            std::for_each( std::cbegin(expired)
                         , std::cend(expired)
                         , f);

            if (!std::empty(expired)) {
               log::write( log::level::info
                         , "Number of expired posts removed: {0}"
                         , std::size(expired));
            }
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

   void on_db_presence(std::string const& user_hash, std::string msg)
   {
      auto const match = sessions_.find(user_hash);
      if (match == std::end(sessions_)) {
         // If the user went offline, we should not be receiving this
         // message. However there may be a timespan where this can
         // happen wo I will simply log.
         log::write(log::level::warning,
	            "Receiving presence after unsubscribe.");
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
      // This function is also called when the connection to redis is
      // lost and restablished. 

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
   void on_db_event( std::vector<std::string> data , redis::response const& req)
   {
      try {
	 switch (req.req)
	 {
	    case redis::events::user_messages:
	       on_db_chat_msg(req.user_id, std::move(data));
	       break;

	    case redis::events::posts:
	       on_db_posts(data);
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

   void on_db_post_connect()
   {
      // There are two situations we have to distinguish here.
      //
      // 1. The server has started.
      //
      // 2. The connection to the database (redis) was lost and restablished.
      //    In this case we have to retrieve all posts that may have arrived
      //    while we were offline.

      auto last = last_post_date_received_;
      ++last;
      db_.retrieve_posts(last.count());
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
   , channel_cfg_ {cfg.ch}
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

   void on_session_dtor( std::string const& user_hash
                       , std::vector<std::string> msgs)
   {
      auto const match = sessions_.find(user_hash);
      if (match == std::end(sessions_))
         return;

      sessions_.erase(match);

      db_.on_user_offline(user_hash);

      if (!std::empty(msgs)) {
         log::write( log::level::debug
                   , "Sending user messages back to the database: {0}"
                   , user_hash);

         db_.store_chat_msg( std::move(user_hash)
                           , std::make_move_iterator(std::begin(msgs))
                           , std::make_move_iterator(std::end(msgs)));
      }
   }

   auto on_app(std::shared_ptr<ws_session_type> s , std::string msg) noexcept
   {
      try {
         auto j = json::parse(msg);
         auto const cmd = j.at("cmd").get<std::string>();

         if (s->is_logged_in()) {
            if (cmd == "presence")
               return on_app_presence(std::move(j), s);
            if (cmd == "message")
               return on_app_chat_msg(std::move(j), s);
            if (cmd == "publish")
               return on_app_publish(std::move(j), s);
         } else {
            if (cmd == "login")
               return on_app_login(j, s);
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

   auto get_stats() const noexcept
   {
      worker_stats wstats {};

      wstats.number_of_sessions = ws_stats_.number_of_sessions;
      wstats.db_post_queue_size = db_.get_post_queue_size();
      wstats.db_chat_queue_size = db_.get_chat_queue_size();

      return wstats;
   }

   template <class UnaryPredicate>
   auto get_posts(date_type date, UnaryPredicate pred) const noexcept
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

      return std::vector<post>{};
   }

   auto count_posts(post const& p) const noexcept
      { return root_channel_.size(); }

   auto search_posts(post const& p) const
   {
      // Later we will use p to find the posts that satisfy the search.
      std::vector<post> items;

      auto pred = [](auto const& p)
         { return true; };

      root_channel_.get_posts(
	 date_type {0},
	 std::back_inserter(items),
	 core_cfg_.max_posts_on_sub,
	 pred);

      return items;
   }

   auto& get_ioc() const noexcept
      { return ioc_; }

   void run()
      { ioc_.run(); }

   auto const& get_cfg() const noexcept
      { return core_cfg_; }

   void delete_post(
      std::string const& user,
      std::string const& key,
      std::string const& post_id)
   {
      // A post should be removed from redis as well as from each
      // worker, so that users do not receive posts from products that
      // are already sold. To delete from the workers it is enough to
      // broadcast a delete command.

      // TODO: Check if the post indeed exists before sending the
      // command to all nodes. This is important to prevent ddos.

      json j;
      j["cmd"] = "delete";
      j["from"] = make_hex_digest(user, key);
      j["post_id"] = post_id;

      db_.remove_post(post_id, j.dump());
   }

   auto get_upload_credit()
   {
      auto f = [this]()
      {
         auto const filename = pwdgen_.make(sz::mms_filename_size);

         auto const path = make_rel_path(filename)
                         + "/"
                         + filename;

         auto const digest = make_hex_digest(path, core_cfg_.mms_key);

         return core_cfg_.mms_host
              + path
              + pwd_gen::sep
              + digest;
      };

      std::vector<std::string> credit;
      std::generate_n(std::back_inserter(credit),
                      sz::mms_filename_size,
                      f);

      return credit;
   }

   void on_visualizations(std::string const& msg)
      { db_.broadcast_on_post(msg); }

   void on_click(std::string const& msg)
      { db_.broadcast_on_post(msg); }

   auto on_publish(json j)
   {
      using namespace std::chrono;

      auto const user = j.at("user").get<std::string>();
      auto const key = j.at("key").get<std::string>();
      auto const user_hash = make_hex_digest(user, key);

      if (std::empty(user_hash)) {
	 json ack;
	 ack["cmd"] = "publish_ack";
	 ack["result"] = "fail";
	 ack["reason"] = "User hash is empty.";
	 return ack.dump();
      }

      auto p = j.at("post").get<post>();

      // TODO: Implement a publication limit by consulting the number
      // of posts the user already has on the channel object.

      // It is important not to thrust the *from* field in the json command.
      p.from = user_hash;
      p.date = duration_cast<seconds>(system_clock::now().time_since_epoch());
      p.id = pwdgen_.make(core_cfg_.pwd_size);

      log::write(log::level::debug, "New post from user {0}", p.from);

      json pub;
      pub["cmd"] = "publish_internal";
      pub["post"] = p;
      db_.post(pub.dump(), p.date.count());

      // It is important that the publisher receives this message before any
      // user sends him a user message about the post. He needs a post_id to
      // know to which post the user refers to.
      json ack;
      ack["cmd"] = "publish_ack";
      ack["result"] = "ok";
      ack["id"] = p.id;
      ack["date"] = p.date.count();
      ack["admin_id"] = "admin-id"; // TODO: Read from config file.

      return ack.dump();
   }

   auto on_get_user_id()
   {
      auto const user = pwdgen_.make(core_cfg_.pwd_size);
      auto const key = pwdgen_.make_key();
      auto const user_hash = make_hex_digest(user, key);

      json resp;
      resp["result"] = "ok";
      resp["user"] = user;
      resp["key"] = key;
      resp["user_id"] = user_hash;

      return resp.dump();
   }
};

}


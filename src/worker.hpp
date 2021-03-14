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
#include <aedis/aedis.hpp>

#include "net.hpp"
#include "post.hpp"
#include "config.hpp"
#include "logger.hpp"
#include "crypto.hpp"
#include "channel.hpp"
#include "ws_session.hpp"
#include "acceptor_mgr.hpp"

namespace occase {

struct ws_stats {
   int number_of_sessions {0};
};

template <class Stream>
class worker : public aedis::receiver_base {
private:
   using ws_session_type = ws_session<Stream>;

   net::io_context ioc_ {BOOST_ASIO_CONCURRENCY_HINT_UNSAFE};
   ssl::context& ctx_;
   config::core const cfg_;
   ws_stats ws_stats_;

   // Used by the redis connection.
   net::ip::tcp::resolver::results_type redis_res_;

   // Maps a user id in to a websocket session.
   std::unordered_map< std::string
                     , std::weak_ptr<ws_session_type>
                     > sessions_;

   channel posts_;
   std::shared_ptr<aedis::connection> redis_conn_;

   // When a user logs in or we receive a notification from the
   // database that there is a message available to the user we issue
   // an lrange + del on the key that holds a list of user messages.
   // When lrange completes we need the user id to forward the
   // message.
   std::queue<std::string> user_ids_chat_queue;

   // The last post id that has beed received from reidis pubsub channel.
   date_type last_post_date_received_ {0};

   // Generates passwords that are sent to the app.
   pwd_gen pwdgen_;

   // Accepts both http connections used by the administrative api or
   // websocket connection used by the database.
   acceptor_mgr<Stream> acceptor_;

   // Signal handler.
   net::signal_set signal_set_;

   void init()
   {
      net::ip::tcp::resolver resolver{ioc_};
      redis_res_ = resolver.resolve(cfg_.redis.host, cfg_.redis.port);
      redis_conn_->start(*this, redis_res_);
   }

   // App functions.
   auto on_app_login(json const& j, std::shared_ptr<ws_session_type> s)
   {
      auto const user = j.at("user").get<std::string>();
      auto const key = j.at("key").get<std::string>();
      auto const user_id = make_hex_digest(user, key);

      log::write( log::level::debug
                , "on_app_login: {0} {1} is logged in."
                , user, user_id);

      if (std::empty(user_id)) {
         json resp;
         resp["cmd"] = "login_ack";
         resp["result"] = "fail";
         s->send(resp.dump(), false);
         return ev_res::login_fail;
      }

      s->set_pub_hash(user_id);

      auto const ss = sessions_.insert({user_id, s});
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
      if (match != std::cend(j)) {
         if (!std::empty(*match)) {
	    // The app sent us a fcm token. We have to
	    //
	    // 1. Publish it in the channel where occase-notify is
	    //    listening
	    //
	    // 2. Add to the redis hash-key where occase-notify can
	    //    load when it is restarted.

	    std::string const token = *match;
            json jtoken;
            jtoken["cmd"] = "token";
            jtoken["user_id"] = user_id;
            jtoken["token"] = token;

            auto const msg = jtoken.dump();

            auto f = [&, this](aedis::request& req)
	    {
	       auto const value = std::make_pair(user_id, token);
	       auto list = {value};

	       req.publish(cfg_.redis.notify_channel, msg);
	       req.hset(cfg_.redis.tokens_key, list);
	    };

            redis_conn_->send(f);
         }
      }

      auto f = [&](aedis::request& req)
      {
         // Instructs redis to notify the worker on new messages to
         // the user. Once a notification arrives the server proceeds
         // with the retrieval of the message, which may be more than
         // one by the time we get to it. Additionaly, this function
         // also subscribes the worker to presence messages.
         req.subscribe(cfg_.redis.user_notify_prefix + user_id);
         req.subscribe(cfg_.redis.presence_channel_prefix + user_id);

         auto const key = cfg_.redis.chat_msg_prefix + user_id;
         req.lrange(key, 0, -1);
         user_ids_chat_queue.push(user_id);

         req.del(key);
         //std::cout << "Sent: " << user_id << std::endl;
      };

      redis_conn_->send(f);

      json resp;
      resp["cmd"] = "login_ack";
      resp["result"] = "ok";
      resp["remaining_posts"] = 0;

      s->send(resp.dump(), false);

      return ev_res::login_ok;
   }

   template <class Iter>
   void
   store_chat_msg(
      Iter begin,
      Iter end,
      std::string const& to)
   {
      if (begin == end)
         return;

      log::write( log::level::debug
                , "store_chat_msg: sending message to {0}"
                , to);

      auto const key = cfg_.redis.chat_msg_prefix + to;
      auto f = [&, this](aedis::request& req)
      {
         req.incr(cfg_.redis.chat_msgs_counter_key);
         req.rpush(key, begin, end);
         req.expire(key, cfg_.redis.chat_msg_exp_time);

         // Notification only of the last message.
         req.publish(cfg_.redis.notify_channel, *std::prev(end));
      };

      redis_conn_->send(f);
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
         auto const msg = j.dump();
         std::initializer_list<std::string_view> list = {msg};
         store_chat_msg(std::cbegin(list), std::cend(list), to);
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
         auto const msg = j.dump();
         auto const channel = cfg_.redis.presence_channel_prefix + to;

         auto f = [&](aedis::request& req)
            { req.publish(channel, msg); };

         redis_conn_->send(f);
      } else {
         if (auto ss = match->second.lock())
            ss->send(j.dump(), false);
      }

      return ev_res::presence_ok;
   }

   auto on_app_publish(json j, std::shared_ptr<ws_session_type> s)
   {
      s->send(on_publish_impl(j), true);
      return ev_res::publish_ok;
   }

   // Handlers for events we receive from the database.
   void on_db_chat_msg( std::string const& user_id
                      , std::vector<std::string> const& msgs)
   {
      auto const match = sessions_.find(user_id);
      if (match == std::end(sessions_)) {
         // The user went offline. We have to enqueue the message
         // again.  This is difficult to test since the retrieval of
         // messages from the database is pretty fast.
         store_chat_msg(std::cbegin(msgs), std::cend(msgs), user_id);
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
      // the map. This is perhaps not a bug but undesirable as we do
      // not have garbage collector for expired sessions.
      assert(false);
   }

   void on_db_channel_post(std::string const& msg)
   {
      using namespace std::chrono;

      try {
         auto const j = json::parse(msg);
         auto const cmd = j.at("cmd").get<std::string>();

         if (cmd == "visualizations") {
            auto const j = json::parse(msg);
            auto post_ids = j.at("post_ids").get<std::vector<std::string>>();
            std::sort(std::begin(post_ids), std::end(post_ids));
            posts_.on_visualizations(post_ids);
            return;
         }

         if (cmd == "click") {
            auto const j = json::parse(msg);
            auto const post_id = j.at("post_id").get<std::string>();
            posts_.on_click(post_id);
            return;
         }

         if (cmd == "delete") {
            auto const post_id = j.at("post_id").get<std::string>();
            auto const from = j.at("from").get<std::string>();
            if (!posts_.remove_post(post_id, from)) 
               log::write( log::level::notice
                         , "Failed to remove post {0}. User {1}"
                         , post_id
                         , from);

            return;
         }

         if (cmd == "publish_internal") {
            auto const item = j.at("post").get<post>();

            if (item.date > last_post_date_received_)
               last_post_date_received_ = item.date;

            auto const now =
               duration_cast<seconds>(system_clock::now().time_since_epoch());
            auto const post_exp = cfg_.timeouts.post_expiration;
            posts_.add_post(item);
            auto const expired = posts_.remove_expired_posts(now, post_exp);

            // NOTE: When we issue the delete command to the other
            // databases, we are in fact also sending a delete cmd to
            // ourselves and in this case the deletions will fail
            // (they have already been removed). This is not bad since
            // it simplifies the code and there are also tipically not
            // so many posts in each deletion, it
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

   void on_db_presence(std::string const& user_id, std::string msg)
   {
      auto const match = sessions_.find(user_id);
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
                   , "worker::shutdown: {0}"
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

      auto g = [](aedis::request& req)
         { req.quit(); };

      redis_conn_->send(g);
   }

public:
   worker(config::core cfg, ssl::context& c)
   : ctx_ {c}
   , cfg_ {cfg}
   , redis_conn_ {std::make_shared<aedis::connection>(ioc_)}
   , acceptor_ {ioc_}
   , signal_set_ {ioc_, SIGINT, SIGTERM}
   {
      auto f = [this](auto const& ec, auto n)
         { on_signal(ec, n); };

      signal_set_.async_wait(f);

      net::post(ioc_, [this]{ init(); });
   }

   // Redis callbacks.
   void on_quit(aedis::resp::simple_string_type& s) noexcept override
   {
      log::write(log::level::debug, "on_quit: {0}", s);
   }

   void on_hello(aedis::resp::map_type& v) noexcept override
   {
      // There are two situations we have to distinguish here.
      //
      // 1. The server has started.
      //
      // 2. The connection to the database (redis) was lost and
      //    restablished.  In this case we have to retrieve all posts
      //    that may have arrived while we were offline.

      log::write( log::level::info
                , "on_hello: connection with Redis stablished.");

      auto last = last_post_date_received_;
      ++last;

      auto f = [&, this](aedis::request& req)
      {
         req.zrangebyscore(cfg_.redis.posts_key, last.count(), -1);
         req.subscribe(cfg_.redis.posts_channel_key);
      };

      redis_conn_->send(f);
   }

   void on_push(aedis::resp::array_type& v) noexcept override
   {
      // Notifications that arrive here have the form.
      //
      //    message pc:6 {"cmd":"presence","from":"5","to":"6","type":"writing"} 
      //    message __keyspace@0__:chat:6 rpush 
      //    message __keyspace@0__:chat:6 expire 
      //    message __keyspace@0__:chat:6 del 
      //    message post_channel {...} 
      //    subscribe posts_channel 1
      //
      // We are only interested in rpush, presence and post_channel
      // messages.
      //
      // Sometimes we will also receive other kind of message like OK when we
      // send the quit command and we have to filter them too. 

      assert(std::size(v) == 3);

      if (v.front() == "message" && v.back() == "rpush") {
         auto const pos = std::size(cfg_.redis.user_notify_prefix);
         auto const user_id = v[1].substr(pos);
         assert(!std::empty(user_id));

         log::write( log::level::debug
                   , "on_push: new chat message to user {0} available."
                   , user_id);

         auto f = [&](aedis::request& req)
         {
            auto const key = cfg_.redis.chat_msg_prefix + user_id;
            req.lrange(key, 0, -1);
            req.del(key);

            // We need the key when lrange completes to forward the
            // message to the user.
            user_ids_chat_queue.push(key);
         };

         redis_conn_->send(f);
         return;
      }

      log::write( log::level::debug
                , "on_push: {0} {1}"
                , v.at(0), v.at(1));

      auto const size = std::size(cfg_.redis.presence_channel_prefix);
      auto const r = v[1].compare(0, size, cfg_.redis.presence_channel_prefix);
      if (v.front() == "message" && r == 0) {
         auto const user_id = v[1].substr(size);
         on_db_presence(user_id, std::move(v.back()));
         return;
      }

      if (v.front() == "message" && v[1] == cfg_.redis.posts_channel_key) {
         on_db_channel_post(v.back());
         return;
      }
   }

   void on_lrange(aedis::resp::array_type& msgs) noexcept override
   {
      assert(!std::empty(user_ids_chat_queue));

      log::write( log::level::debug
                , "on_lrange: received messages from {0}"
                , user_ids_chat_queue.front());

      on_db_chat_msg(user_ids_chat_queue.front(), msgs);
      user_ids_chat_queue.pop();
   }

   void on_zrangebyscore(aedis::resp::array_type& msgs) noexcept override
   {
      log::write( log::level::info
                , "on_zrangebyscore: {0} messages received."
                , std::size(msgs));

      auto loader = [this](auto const& msg)
         { on_db_channel_post(msg); };

      std::for_each(std::begin(msgs), std::end(msgs), loader);

      if (!acceptor_.is_open()) {
         // We can start to accept websocket connections.
         acceptor_.run( *this
                      , ctx_
                      , cfg_.db_port
                      , cfg_.max_listen_connections);
      }
   }

   void on_session_dtor( std::string const& user_id
                       , std::vector<std::string> const& msgs)
   {
      auto const match = sessions_.find(user_id);
      if (match == std::end(sessions_))
         return;

      sessions_.erase(match);

      // Usubscribe to the notifications to the key. On completion it
      // passes no event to the worker.
      auto f = [&](aedis::request& req)
      {
         req.unsubscribe(cfg_.redis.user_notify_prefix + user_id);
         req.unsubscribe(cfg_.redis.presence_channel_prefix + user_id);
      };

      redis_conn_->send(f);

      if (!std::empty(msgs)) {
         log::write( log::level::debug
                   , "Sending user messages back to the database: {0}"
                   , user_id);

         store_chat_msg(std::cbegin(msgs), std::cend(msgs), user_id);
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
         log::write(log::level::debug, "worker::on_app: {0}.", e.what());
      }

      return ev_res::unknown;
   }

   auto const& get_timeouts() const noexcept
      { return cfg_.timeouts;}
   auto& get_ws_stats() noexcept
      { return ws_stats_;}
   auto const& get_ws_stats() const noexcept
      { return ws_stats_; }

   auto get_stats() const noexcept
   {
      worker_stats wstats {};

      wstats.number_of_sessions = ws_stats_.number_of_sessions;
      wstats.db_post_queue_size = 0;
      wstats.db_chat_queue_size = std::size(user_ids_chat_queue);

      return wstats;
   }

   template <class UnaryPredicate>
   auto get_posts(date_type date, UnaryPredicate pred) const noexcept
   {
      try {
         std::vector<post> posts;

         posts_.get_posts(
            date,
            std::back_inserter(posts),
            cfg_.max_posts_on_search,
            pred);

         return posts;
      } catch (std::exception const& e) {
         log::write(log::level::info, "get_posts: {}", e.what());
      }

      return std::vector<post>{};
   }

   auto count_posts(post const& p) const noexcept
      { return posts_.size(); }

   auto search_posts(post const& p) const
   {
      // Later we will use p to find the posts that satisfy the search.
      std::vector<post> items;

      auto pred = [](auto const& p)
         { return true; };

      posts_.get_posts(
         date_type {0},
         std::back_inserter(items),
         cfg_.max_posts_on_search,
         pred);

      return items;
   }

   auto& get_ioc() const noexcept
      { return ioc_; }

   void run()
      { ioc_.run(); }

   auto const& get_cfg() const noexcept
      { return cfg_; }

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

      // TODO: For safety I will not remove from redis.  For example with
      // zremrangebyscore. Decide what to do here.

      json j;
      j["cmd"] = "delete";
      j["from"] = make_hex_digest(user, key);
      j["post_id"] = post_id;

      auto const msg = j.dump();

      auto f = [&](aedis::request& req)
         { req.publish(cfg_.redis.posts_channel_key, msg);};

      redis_conn_->send(f);
   }

   auto get_upload_credit()
   {
      auto f = [this]()
      {
         auto const filename = pwdgen_.make(sz::mms_filename_size);

         auto const path = make_rel_path(filename)
                         + "/"
                         + filename;

         auto const digest = make_hex_digest(path, cfg_.mms_key);

         return cfg_.mms_host
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
   {
      auto f = [&](aedis::request& req)
         { req.publish(cfg_.redis.posts_channel_key, msg); };

      redis_conn_->send(f);
   }

   void on_click(std::string const& msg)
   {
      auto f = [&](aedis::request& req)
         { req.publish(cfg_.redis.posts_channel_key, msg); };

      redis_conn_->send(f);
   }

   auto on_publish_impl(json j)
   {
      using namespace std::chrono;

      auto const user = j.at("user").get<std::string>();
      auto const key = j.at("key").get<std::string>();
      auto const user_id = make_hex_digest(user, key);

      if (std::empty(user_id)) {
         json ack;
         ack["cmd"] = "publish_ack";
         ack["result"] = "fail";
         ack["reason"] = "Invalid user id.";
         return ack.dump();
      }

      auto p = j.at("post").get<post>();

      // TODO: Implement a publication limit by consulting the number
      // of posts the user already has on the channel object.

      // It is important not to thrust the *from* field in the json command.
      p.from = user_id;
      p.date = duration_cast<seconds>(system_clock::now().time_since_epoch());
      p.id = pwdgen_.make(cfg_.pwd_size);

      log::write(
         log::level::debug,
         "on_publish_impl: new post from user {0}",
         p.from);

      json pub;
      pub["cmd"] = "publish_internal";
      pub["post"] = p;
      auto const msg = pub.dump();

      auto f = [&, this](aedis::request& req)
      {
         req.zadd(cfg_.redis.posts_key, p.date.count(), msg);
         req.publish(cfg_.redis.posts_channel_key, msg);
      };

      redis_conn_->send(f);

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
      auto const user = pwdgen_.make(cfg_.pwd_size);
      auto const key = pwdgen_.make_key();
      auto const user_id = make_hex_digest(user, key);

      json resp;
      resp["result"] = "ok";
      resp["user"] = user;
      resp["key"] = key;
      resp["user_id"] = user_id;

      return resp.dump();
   }
};

}


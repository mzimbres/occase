#pragma once

#include <array>
#include <vector>
#include <memory>
#include <string>
#include <unordered_map>

#include <boost/asio.hpp>
#include <boost/asio/signal_set.hpp>

#include <aedis/aedis.hpp>

#include "net.hpp"
#include "post.hpp"
#include "config.hpp"
#include "logger.hpp"
#include "crypto.hpp"
#include "channel.hpp"
#include "acceptor_mgr.hpp"
#include "ws_session_base.hpp"

namespace occase {

struct ws_stats {
   int number_of_sessions {0};
};

struct worker_stats {
   int number_of_sessions = 0;
   int worker_post_queue_size = 0;
   int worker_reg_queue_size = 0;
   int worker_login_queue_size = 0;
   int db_post_queue_size = 0;
   int db_chat_queue_size = 0;
};

std::ostream& operator<<(std::ostream& os, worker_stats const& stats);
std::string to_string(worker_stats const& stats);

class worker : public aedis::receiver_base {
private:
   net::io_context ioc_ {BOOST_ASIO_CONCURRENCY_HINT_UNSAFE};
   ssl::context& ctx_;
   config::core const cfg_;
   ws_stats ws_stats_;

   // Maps a user id in to a websocket session.
   std::unordered_map< std::string
                     , std::weak_ptr<ws_session_base>
                     > sessions_;

   channel posts_;
   std::shared_ptr<aedis::connection> redis_conn_;

   // When a user logs in or we receive a notification from the
   // database that there is a message available to the user we issue
   // an lrange + del on the key that holds a list of user messages.
   // When lrange completes we need the user id to forward the
   // message.
   std::queue<std::string> user_ids_chat_queue;

   // Generates passwords that are sent to the app.
   pwd_gen pwdgen_;

   // Accepts both http connections used by the administrative api or
   // websocket connection used by the database.
   acceptor_mgr acceptor_;

   // Signal handler.
   net::signal_set signal_set_;

private:
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

   void init();
   ev_res on_app_login(json const& j, std::shared_ptr<ws_session_base> s);
   ev_res on_app_chat_msg(json j, std::shared_ptr<ws_session_base> s);
   ev_res on_app_presence(json j, std::shared_ptr<ws_session_base> s);
   ev_res on_app_publish(json j, std::shared_ptr<ws_session_base> s);
   void on_db_chat_msg( std::string const& user_id, std::vector<std::string> const& msgs);
   void on_db_channel_post(std::string const& msg);
   void on_db_presence(std::string const& user_id, std::string msg);
   void on_signal(boost::system::error_code const& ec, int n);

   // WARNING: Don't call this function from the signal handler.
   void shutdown();
   void shutdown_impl();
   std::string get_chat_to_field(json& j, std::string& to);

public:
   worker(config::core cfg, ssl::context& c);

   // Redis receiver functions
   void on_quit(aedis::resp::simple_string_type& s) noexcept override;
   void on_hello(aedis::resp::map_type& v) noexcept override;
   void on_push(aedis::resp::array_type& v) noexcept override;
   void on_lrange(aedis::resp::array_type& msgs) noexcept override;
   void on_hvals(aedis::resp::array_type& msgs) noexcept override;
   void on_hgetall(aedis::resp::array_type& all) noexcept override;

   void on_session_dtor( std::string const& user_id, std::vector<std::string> const& msgs);
   ev_res on_app(std::shared_ptr<ws_session_base> s , std::string msg) noexcept;
   auto const& get_timeouts() const noexcept { return cfg_.timeouts;}
   auto& get_ws_stats() noexcept { return ws_stats_;}
   auto const& get_ws_stats() const noexcept { return ws_stats_; }
   worker_stats get_stats() const noexcept;
   int count_posts(post const& p) const;
   std::vector<post> search_posts(post const& p) const;
   auto& get_ioc() const noexcept { return ioc_; }
   void run() { ioc_.run(); }
   auto const& get_cfg() const noexcept { return cfg_; }
   void delete_post( std::string const& user, std::string const& key, std::string const& post_id);
   std::vector<std::string> get_upload_credit();
   void on_visualization(std::string const& msg);
   std::string on_publish_impl(json j);
   std::string on_get_user_id();
};

} // occase


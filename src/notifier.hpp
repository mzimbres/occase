#pragma once

#include <unordered_map>

#include <aedis/aedis.hpp>
#include <nlohmann/json.hpp>

#include "net.hpp"
#include "logger.hpp"
#include "ntf_session.hpp"

using json = nlohmann::json;

namespace occase
{

struct user_entry {
   // User FCM token.
   std::string token;

   // The message that has been sent to the user.
   json msg;

   boost::asio::steady_timer timer;

   auto has_token() const noexcept
      { return !std::empty(token); }
};

class notifier : public aedis::receiver_base {
public:
   std::string const redis_rpush_ntf {"__keyevent@0__:rpush"};
   std::string const redis_del_ntf {"__keyevent@0__:del"};

   struct config {
      std::string ssl_cert_file;
      std::string ssl_priv_key_file;
      std::string ssl_dh_file;

      std::string redis_notify_channel;
      std::string redis_tokens_key;
      std::string redis_host;
      std::string redis_port;
      int redis_max_pipeline_size = 1024;

      ntf_session::config ntf;
      int max_msg_size = 1000;

      // The interval we are willing to wait for occase-db to retrieve
      // the message from redis before we consider the user offline.
      int wait_interval = 5;

      auto get_wait_interval() const noexcept
	 { return std::chrono::seconds {wait_interval}; }
   };

   using map_type = std::unordered_map<std::string, user_entry>;

private:
   net::io_context ioc_ {BOOST_ASIO_CONCURRENCY_HINT_UNSAFE};
   config cfg_;
   ssl::context ctx_ {ssl::context::tlsv12};
   tcp::resolver::results_type fcm_results_;
   std::shared_ptr<aedis::connection> redis_conn_;

   // Maps the key holding the user messages in redis into an FCM
   // token.
   map_type tokens_;

   void on_hgetall(aedis::resp::array_type& set) noexcept override final;

   void on_push(aedis::resp::array_type& v) noexcept override final;

   void on_simple_error(
      aedis::command cmd,
      aedis::resp::simple_error_type& v) noexcept override final;

   void on_blob_error(
      aedis::command cmd,
      aedis::resp::blob_error_type& v) noexcept override final;

   void on_null(aedis::command cmd) noexcept override final;

   void init();
   void on_ntf_message(json j);
   void on_ntf_del(std::string const& key);
   void on_ntf_publish(std::string const& token);

   void on_timeout(
      boost::system::error_code ec,
      map_type::const_iterator iter) noexcept;

public:
   notifier(config const& cfg);
   void run();
};

}


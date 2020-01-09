#pragma once

#include <unordered_map>

#include <nlohmann/json.hpp>
#include <aedis/aedis.hpp>

#include "net.hpp"
#include "utils.hpp"
#include "logger.hpp"
#include "ntf_session.hpp"

using json = nlohmann::json;

namespace occase
{

struct user_entry {
   // Contains the user fcm token.
   std::string token;

   // The message that has been sent to the user.
   json msg;

   boost::asio::steady_timer timer;

   auto has_token() const noexcept
      { return !std::empty(token); }
};

class notifier {
public:
   const std::string rpush_str {"__keyevent@0__:rpush"};
   const std::string del_str {"__keyevent@0__:del"};

   struct config {
      std::string ssl_cert_file;
      std::string ssl_priv_key_file;
      std::string ssl_dh_file;
      std::string redis_token_channel;
      std::string tokens_file;
      aedis::session::config ss;
      ntf_session::args ss_args;
      int max_msg_size = 1000;
      int tokens_write_interval = 60;

      // The interval we are willing to wait for occase-db to retrieve the
      // message from redis before we consider the user offline.
      int wait_interval = 5;

      auto get_wait_interval() const noexcept
      {
         return std::chrono::seconds {wait_interval};
      }

      auto get_tokens_write_interval() const noexcept
      {
         return std::chrono::seconds {tokens_write_interval};
      }
   };

   using map_type = std::unordered_map<std::string, user_entry>;

private:
   net::io_context ioc_ {BOOST_ASIO_CONCURRENCY_HINT_UNSAFE};
   config cfg_;
   ssl::context ctx_ {ssl::context::tlsv12};
   aedis::session ss_;
   tcp::resolver::results_type fcm_results_;

   // Maps the key holding the user messages in redis in an fcm token.
   map_type tokens_;

   boost::asio::steady_timer tokens_file_timer_;

   void on_db_event(
      boost::system::error_code ec,
      std::vector<std::string> resp);

   void on_db_conn();
   void init();
   void on_message(json j);
   void on_del(std::string const& key);
   void on_publish(std::string const& token);
   void on_timeout(
      boost::system::error_code ec,
      map_type::const_iterator iter) noexcept;

public:
   notifier(config const& cfg);
   void run();
};

}


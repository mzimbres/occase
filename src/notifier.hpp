#pragma once

#include <unordered_map>

#include <aedis/aedis.hpp>

#include "net.hpp"
#include "utils.hpp"
#include "logger.hpp"

namespace occase
{

struct notifier_data {
   std::string token {"Test"};
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
      std::string fcm_server_token;
      std::string fcm_server_url;
      std::string redis_token_channel;
      aedis::session::config ss;

      // The interval we are willing to wait for occase-db to retrieve the
      // message from redis before we consider the user offline.
      int wait_interval = 5;

      auto get_wait_interval() const noexcept
      {
         return std::chrono::seconds {wait_interval};
      }

      auto with_ssl() const noexcept
      {
         auto const r =
            std::empty(ssl_cert_file) ||
            std::empty(ssl_priv_key_file) ||
            std::empty(ssl_dh_file);

         return !r;
      }

      auto fcm_valid() const noexcept
      {
         auto const r =
            std::empty(fcm_server_token) ||
            std::empty(fcm_server_url);

         return !r;
      }
   };

   using map_type = std::unordered_map<std::string, notifier_data>;

private:
   net::io_context ioc_ {1};
   config cfg_;
   ssl::context ctx_ {ssl::context::tlsv12};
   aedis::session ss_;

   // Maps the key holding the user messages in redis in an fcm token.
   map_type tokens_;

   void on_db_event(
      boost::system::error_code ec,
      std::vector<std::string> resp);

   void on_db_conn();
   void init();
   void on_rpush(std::string const& key);
   void on_del(std::string const& key);
   void on_token(std::string const& token);

public:
   notifier(config const& cfg);
   void run();
};

}


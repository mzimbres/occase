#pragma once

#include <stack>
#include <vector>
#include <memory>
#include <string>
#include <unordered_map>

#include <boost/asio.hpp>
#include <boost/asio/steady_timer.hpp>

#include "config.hpp"
#include "channel.hpp"
#include "json_utils.hpp"
#include "redis_session.hpp"

namespace rt
{

class server_session;

enum class ev_res
{ register_ok
, register_fail
, login_ok
, login_fail
, code_confirmation_ok
, code_confirmation_fail
, subscribe_ok
, subscribe_fail
, unsubscribe_ok
, unsubscribe_fail
, publish_ok
, publish_fail
, user_msg_ok
, user_msg_fail
, unknown
};

struct session_timeouts {
   std::chrono::seconds auth {2};
   std::chrono::seconds code {2};
   std::chrono::seconds handshake {2};
   std::chrono::seconds pong {2};
   std::chrono::seconds close {2};
};

struct server_mgr_cf {
   std::string redis_address;
   std::string redis_port;
   std::string redis_group_channel;

   int auth_timeout;
   int code_timeout;
   int handshake_timeout;
   int pong_timeout;
   int close_frame_timeout;

   std::string redis_menu_key;
   std::string redis_msg_prefix;

   auto get_timeouts() const noexcept
   {
      return session_timeouts
      { std::chrono::seconds {auth_timeout}
      , std::chrono::seconds {code_timeout}
      , std::chrono::seconds {handshake_timeout}
      , std::chrono::seconds {pong_timeout}
      , std::chrono::seconds {close_frame_timeout}
      };
   }

   auto get_redis_session_cf()
   {
      return redis_session_cf {redis_address, redis_port};
   }
};

struct sessions_stats {
   int number_of_sessions {0};
};

class server_mgr {
private:
   net::io_context& ioc;
   // Maps a user id (telephone, email, etc.) to a user obj.
   std::unordered_map< std::string
                     , std::weak_ptr<server_session>> sessions;

   // Maps a channel id to the corresponding channel object.
   std::unordered_map<std::string, channel> channels;

   session_timeouts const timeouts;
   sessions_stats stats;
   redis_session redis_gsub_session;
   redis_session redis_ksub_session;
   redis_session redis_pub_session;
   std::string const redis_group_channel;
   std::string const redis_menu_key;
   std::string const redis_keyspace_prefix {"__keyspace@0__:"};
   std::string const redis_msg_prefix;
   std::string const redis_notify_prefix;
   std::string menu;

   net::steady_timer stats_timer;

   void redis_group_msg_handler( boost::system::error_code const& ec
                               , std::vector<std::string> const& resp
                               , redis_req const& cmd);
   void redis_key_msg_handler( boost::system::error_code const& ec
                             , std::vector<std::string> const& resp
                             , redis_req const& cmd);
   void redis_pub_msg_handler( boost::system::error_code const& ec
                             , std::vector<std::string> const& resp
                             , redis_req const& cmd);
   void do_stats_logger();

public:
   server_mgr(server_mgr_cf cf, net::io_context& ioc);
   void shutdown();
   void release_auth_session(std::string const& id);

   ev_res on_register(json j, std::shared_ptr<server_session> s);
   ev_res on_login(json j, std::shared_ptr<server_session> s);
   ev_res on_code_confirmation(json j, std::shared_ptr<server_session> s);
   ev_res on_subscribe(json j, std::shared_ptr<server_session> session);
   ev_res on_unsubscribe(json j, std::shared_ptr<server_session> session);
   ev_res on_user_msg( std::string msg, json j
                     , std::shared_ptr<server_session> session);
   ev_res on_publish( std::string msg, json j
                    , std::shared_ptr<server_session> session);

   auto const& get_timeouts() const noexcept {return timeouts;}
   auto& get_stats() noexcept {return stats;}
   auto& get_io_context() {return ioc;}
};

ev_res on_message( server_mgr& mgr
                 , std::shared_ptr<server_session> s
                 , std::string msg);

}


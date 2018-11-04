#pragma once

#include <stack>
#include <vector>
#include <memory>
#include <string>
#include <unordered_map>

#include <boost/asio.hpp>
#include <boost/asio/steady_timer.hpp>

#include "config.hpp"
#include "json_utils.hpp"
#include "redis_session.hpp"

namespace rt
{

class server_session;

enum class ev_res
{ login_ok
, login_fail
, auth_ok
, auth_fail
, sms_confirmation_ok
, sms_confirmation_fail
, create_group_ok
, create_group_fail
, join_group_ok
, join_group_fail
, group_msg_ok
, group_msg_fail
, user_msg_ok
, user_msg_fail
, unknown
};

struct session_timeouts {
   std::chrono::seconds auth {2};
   std::chrono::seconds sms {2};
   std::chrono::seconds handshake {2};
   std::chrono::seconds pong {2};
   std::chrono::seconds close {2};
};

struct server_mgr_cf {
   std::string redis_address;
   std::string redis_port;
   std::string redis_group_channel;

   int auth_timeout;
   int sms_timeout;
   int handshake_timeout;
   int pong_timeout;
   int close_frame_timeout;

   std::string redis_menu_key;
   std::string redis_msg_prefix;

   auto get_timeouts() const noexcept
   {
      return session_timeouts
      { std::chrono::seconds {auth_timeout}
      , std::chrono::seconds {sms_timeout}
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

// The number of members in a channel is expected be on the
// thousands, let us say 10k. The operations performed are
//
// 1. Insert very often. Can be on the back.
// 2. Traverse very often.
// 3. Remove: Once in a while, requires searching.
// 
// Would like to use a vector but cannot pay for the linear search
// time, even if not occurring very often.
using channel_type =
   std::unordered_map< std::string
                     , std::weak_ptr<server_session>>;

class server_mgr {
private:
   // Maps a user id (telephone, email, etc.) to a user obj.
   std::unordered_map< std::string
                     , std::weak_ptr<server_session>> sessions;

   // Maps a channel id to a map of server sessions that subscribed to
   // that channel.
   std::unordered_map<std::string, channel_type> channels;

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

   boost::asio::steady_timer stats_timer;

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
   server_mgr(server_mgr_cf cf, asio::io_context& ioc);
   void shutdown();
   void release_auth_session(std::string const& id);

   ev_res on_login(json j, std::shared_ptr<server_session> s);
   ev_res on_auth(json j, std::shared_ptr<server_session> s);
   ev_res on_sms_confirmation(json j, std::shared_ptr<server_session> s);
   ev_res on_create_group(json j, std::shared_ptr<server_session> s);
   ev_res on_join_group(json j, std::shared_ptr<server_session> session);
   ev_res on_user_msg( std::string msg, json j
                     , std::shared_ptr<server_session> session);
   ev_res on_user_group_msg( std::string msg, json j
                             , std::shared_ptr<server_session> session);

   auto const& get_timeouts() const noexcept {return timeouts;}
   auto& get_stats() noexcept {return stats;}
};

ev_res on_message( server_mgr& mgr
                 , std::shared_ptr<server_session> s
                 , std::string msg);

}


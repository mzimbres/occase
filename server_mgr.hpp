#pragma once

#include <vector>
#include <memory>
#include <string>
#include <unordered_map>

#include "config.hpp"
#include "json_utils.hpp"

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
   unsigned short redis_port;
   int auth_timeout;
   int sms_timeout;
   int handshake_timeout;
   int pong_timeout;
   int close_frame_timeout;

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

public:
   server_mgr(server_mgr_cf cf)
   : timeouts(cf.get_timeouts())
   {}
   void shutdown();
   void release_user(std::string id);

   ev_res on_login(json j, std::shared_ptr<server_session> s);
   ev_res on_auth(json j, std::shared_ptr<server_session> s);
   ev_res on_sms_confirmation(json j, std::shared_ptr<server_session> s);
   ev_res on_create_group(json j, std::shared_ptr<server_session> s);
   ev_res on_join_group(json j, std::shared_ptr<server_session> session);
   ev_res on_user_msg(json j, std::shared_ptr<server_session> session);
   ev_res on_group_msg( std::string msg
                      , json j // To avoid parsing it again.
                      , std::shared_ptr<server_session> session);

   auto const& get_timeouts() const noexcept {return timeouts;}
   auto& get_stats() noexcept {return stats;}
};

ev_res on_message( server_mgr& mgr
                 , std::shared_ptr<server_session> s
                 , std::string msg);


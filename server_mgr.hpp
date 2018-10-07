#pragma once

#include <mutex>
#include <vector>
#include <memory>
#include <string>
#include <unordered_map>

#include "config.hpp"
#include "group.hpp"

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

struct sessions_stats {
   std::atomic<int> number_of_sessions {0};
};

class server_mgr {
private:
   std::mutex mutex;

   // Maps a user id (telephone, email, etc.) to a user obj.
   std::unordered_map< std::string
                     , std::weak_ptr<server_session>> sessions;

   // Maps a group id to a group object.
   std::unordered_map<std::string, group> groups;

   session_timeouts const timeouts;
   sessions_stats stats;

public:
   server_mgr(session_timeouts timeouts_)
   : timeouts(timeouts_)
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


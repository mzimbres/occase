#pragma once

#include <vector>
#include <memory>
#include <unordered_map>

#include "config.hpp"
#include "user.hpp"
#include "group.hpp"
#include "idx_mgr.hpp"

class server_session;

enum class ev_res
{ LOGIN_OK
, LOGIN_FAIL
, AUTH_OK
, AUTH_FAIL
, SMS_CONFIRMATION_OK
, SMS_CONFIRMATION_FAIL
, CREATE_GROUP_OK
, CREATE_GROUP_FAIL
, JOIN_GROUP_OK
, JOIN_GROUP_FAIL
, GROUP_MSG_OK
, GROUP_MSG_FAIL
, USER_MSG_OK
, USER_MSG_FAIL
, UNKNOWN
};

inline auto drop_session(ev_res r) noexcept
{
   switch (r) {
   case ev_res::LOGIN_OK:              return false;
   case ev_res::LOGIN_FAIL:            return true;
   case ev_res::AUTH_OK:               return false;
   case ev_res::AUTH_FAIL:             return true;
   case ev_res::SMS_CONFIRMATION_OK:   return false;
   case ev_res::SMS_CONFIRMATION_FAIL: return true;
   case ev_res::CREATE_GROUP_OK:       return false;
   case ev_res::CREATE_GROUP_FAIL:     return false;
   case ev_res::JOIN_GROUP_OK:         return false;
   case ev_res::JOIN_GROUP_FAIL:       return false;
   case ev_res::GROUP_MSG_OK:          return false;
   case ev_res::GROUP_MSG_FAIL:        return false;
   case ev_res::USER_MSG_OK:           return false;
   case ev_res::USER_MSG_FAIL:         return false;
   default:                            return true;
   }
}

class server_mgr {
private:
   std::string host = "criatura";

   // Maps a user id (telephone, email, etc.) to a user index in the
   // users vector.
   std::unordered_map<id_type, index_type> id_to_idx_map;
   std::vector<user> users;
   idx_mgr user_idx_mgr;

   // Maps a group hash to a group object.
   std::unordered_map<std::string, group> groups;

   ev_res on_login(json j, std::shared_ptr<server_session> s);
   ev_res on_auth(json j, std::shared_ptr<server_session> s);
   ev_res on_sms_confirmation(json j, std::shared_ptr<server_session> s);
   ev_res on_create_group(json j, std::shared_ptr<server_session> s);
   ev_res on_join_group(json j, std::shared_ptr<server_session> session);
   ev_res on_group_msg(json j, std::shared_ptr<server_session> session);
   ev_res on_user_msg(json j, std::shared_ptr<server_session> session);

public:
   server_mgr(int users_size)
   : users(users_size)
   , user_idx_mgr(users_size)
   {}

   // Functions to interact with the server_session.
   ev_res on_read(json j, std::shared_ptr<server_session> session);
   void on_write(index_type user_idx);
   void release_login(index_type idx);
};


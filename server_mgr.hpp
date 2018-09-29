#pragma once

#include <vector>
#include <memory>
#include <string>
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

class server_mgr {
private:
   std::string host = "criatura";

   // Maps a user id (telephone, email, etc.) to a user obj.
   std::unordered_map<std::string, user> users;

   // Maps a group id to a group object.
   std::unordered_map<std::string, group> groups;

   ev_res on_login(json j, std::shared_ptr<server_session> s);
   ev_res on_auth(json j, std::shared_ptr<server_session> s);
   ev_res on_sms_confirmation(json j, std::shared_ptr<server_session> s);
   ev_res on_create_group(json j, std::shared_ptr<server_session> s);
   ev_res on_join_group(json j, std::shared_ptr<server_session> session);
   ev_res on_group_msg( std::string msg
                      , json j // To avoid parsing it again.
                      , std::shared_ptr<server_session> session);
   ev_res on_user_msg(json j, std::shared_ptr<server_session> session);

public:
   ev_res on_read( std::string msg
                 , std::shared_ptr<server_session> session);
   void shutdown();
   void release_user(std::string id);
};


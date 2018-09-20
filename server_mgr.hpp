#pragma once

#include <vector>
#include <memory>
#include <unordered_map>

#include "config.hpp"
#include "user.hpp"
#include "group.hpp"
#include "idx_mgr.hpp"

class server_session;

class server_mgr {
private:
   std::string host = "criatura";

   // Maps a user id (telephone, email, etc.) to a user index in the
   // users vector.
   std::unordered_map<id_type, index_type> id_to_idx_map;
   std::vector<user> users;
   idx_mgr user_idx_mgr;;

   // Maps a group hash to a group object.
   std::unordered_map<std::string, group> groups;

   index_type on_login(json j, std::shared_ptr<server_session> s);
   index_type on_auth(json j, std::shared_ptr<server_session> s);
   index_type on_sms_confirmation(json j, std::shared_ptr<server_session> s);
   index_type on_create_group(json j, std::shared_ptr<server_session> s);
   index_type on_join_group(json j, std::shared_ptr<server_session> session);
   index_type on_group_msg(json j, std::shared_ptr<server_session> session);
   index_type on_user_msg(json j, std::shared_ptr<server_session> session);

public:
   server_mgr(int users_size)
   : users(users_size)
   , user_idx_mgr(users_size)
   {}

   // Functions to interact with the server_session.
   index_type on_read(json j, std::shared_ptr<server_session> session);
   void on_write(index_type user_idx);
   void release_login(index_type idx);
};


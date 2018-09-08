#pragma once

#include <vector>
#include <memory>
#include <unordered_map>

#include "config.hpp"
#include "user.hpp"
#include "group.hpp"
#include "grow_only_vector.hpp"

class server_session;

class server_mgr {
private:
   std::string host = "criatura";
   std::unordered_map<id_type, index_type> id_to_idx_map;
   grow_only_vector<user> users;
   std::unordered_map<std::string, group> groups;

   index_type on_login(json j, std::shared_ptr<server_session> s);
   index_type on_auth(json j, std::shared_ptr<server_session> s);
   index_type on_sms_confirmation(json j, std::shared_ptr<server_session> s);
   index_type on_create_group(json j, std::shared_ptr<server_session> s);
   index_type on_join_group(json j, std::shared_ptr<server_session> session);
   index_type on_group_msg(json j, std::shared_ptr<server_session> session);
   index_type on_user_msg(json j, std::shared_ptr<server_session> session);

public:
   server_mgr(int users_size, int groups_size)
   : users(users_size)
   {}

   // Functions to interact with the server_session.
   index_type on_read(json j, std::shared_ptr<server_session> session);

   void on_write(index_type user_idx);
   void release_login(index_type idx);
};


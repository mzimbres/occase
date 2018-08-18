#pragma once

#include <vector>
#include <memory>
#include <unordered_map>

#include "config.hpp"
#include "user.hpp"
#include "group.hpp"
#include "grow_only_vector.hpp"

class server_session;

class server_data {
private:
   grow_only_vector<user> users;
   grow_only_vector<group> groups;

   // May grow up to millions of users.
   std::unordered_map<id_type, index_type> id_to_idx_map;

public:
   server_data(int users_size, int groups_size)
   : users(users_size)
   , groups(groups_size)
   {}

   // This function is used to add a new user when he first installs
   // the app and sends the first message to the server.  It basically
   // allocates user entries internally.
   index_type on_login(json j, std::shared_ptr<server_session> s);

   index_type on_create_group(json j, std::shared_ptr<server_session> s);

   // Removes the group and updates the owner.
   group remove_group(index_type idx);

   bool change_group_ownership( index_type from, index_type to
                              , index_type gid);

   index_type on_join_group(json j, std::shared_ptr<server_session> session);

   index_type on_group_msg(json j, std::shared_ptr<server_session> session);

   index_type on_user_msg(json j, std::shared_ptr<server_session> session);

   index_type on_read(json j, std::shared_ptr<server_session> session);
};


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
   grow_only_vector<group> groups;
   grow_only_vector<user> users;

   // May grow up to millions of users.
   std::unordered_map<id_type, index_type> id_to_idx_map;

public:

   // This function is used to add a new user when he first installs
   // the app and it sends the first message to the server.  It
   // basically allocates user entries internally.
   //
   // id:       User telephone.
   // s:        Web socket session.
   // return:   Index in the users vector that we will use to refer to
   //           him without having to perform searches. This is what
   //           we will return to the user to be stored in his app.
   index_type add_user( id_type id
                      , std::shared_ptr<server_session> s);

   // TODO: update_user_contacts.

   // Creates a new group for the specified owner and returns its
   // index.
   index_type create_group(index_type owner);

   // Removes the group and updates the owner.
   group remove_group(index_type idx);

   bool change_group_ownership( index_type from, index_type to
                              , index_type gid);

   bool add_group_member( index_type owner, index_type new_member
                        , index_type gid);
};


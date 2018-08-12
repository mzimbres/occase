#include "server_data.hpp"

index_type
server_data::add_user( id_type id
                     , std::shared_ptr<server_session> session)
{
   index_type new_user_idx = -1;
   auto new_user = id_to_idx_map.insert({id, {}});
   if (!new_user.second) {
      // The user already exists in the system. This case can be
      // triggered by some conditions
      // 1. The user lost his phone and the number was assigned to
      //    a new person.
      // 2. He reinstalled the app.
      //
      // REVIEW: I still do not know how to handle this sitiations
      // properly so I am not going to do anything for now. We
      // could for example reset his data.
      new_user_idx = new_user.first->second;
   } else {
      // The user did not exist in the system. We have to allocate
      // space for him.
      new_user_idx = users.allocate();
   }

   // Set users websocket session.
   if (!users[new_user_idx].has_session())
      users[new_user_idx].set_session(session);

   return new_user_idx;
}

index_type server_data::create_group(index_type owner)
{
   // Before allocating a new group it is a good idea to check if
   // the owner passed is at least in a valid range.
   if (!users.is_valid_index(owner)) {
      // This is a non-existing user. Perhaps the json command was
      // sent with the wrong information signaling a logic error in
      // the app.
      return static_cast<index_type>(-1);
   }

   // We can proceed and allocate the group.
   auto idx = groups.allocate();
   groups[idx].set_owner(owner);

   // Updates the user with his new group.
   users[owner].add_group(idx);

   return idx;
}

group server_data::remove_group(index_type idx)
{
   if (!groups.is_valid_index(idx))
      return group {}; // Out of range? Logic error.

   // To remove a group we have to inform its members the group has
   // been removed, therefore we will make a copy before removing
   // and return it.
   const auto removed_group = std::move(groups[idx]);

   // remove this line after implementing the swap idiom on the
   // group class.
   groups[idx].reset();

   groups.deallocate(idx);

   // Now we have to remove this group from the owners list.
   const auto owner = removed_group.get_owner();
   users[owner].remove_group(idx);
   return removed_group; // Let RVO optimize this.
}

bool server_data::change_group_ownership( index_type from, index_type to
                                        , index_type gid)
{
   if (!groups.is_valid_index(gid))
      return false;

   if (!groups[gid].is_owned_by(from))
      return false; // Sorry, you are not allowed.

   if (!users.is_valid_index(from))
      return false;

   if (!users.is_valid_index(to))
      return false;

   // The new owner exists.
   groups[gid].set_owner(to);
   users[to].add_group(gid);
   users[from].remove_group(gid);
   return true;
}

bool server_data::add_group_member( index_type owner, index_type new_member
                                  , index_type gid)
{
   if (!groups.is_valid_index(gid))
      return false;

   if (!groups[gid].is_owned_by(owner))
      return false;

   if (!users.is_valid_index(owner))
      return false;

   if (!users.is_valid_index(new_member))
      return false;
      
   groups[gid].add_member(new_member);

   return true;
}


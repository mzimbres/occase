#include <iostream>

#include <stack>
#include <vector>
#include <algorithm>
#include <unordered_map>

using uid_type = long long;
using gid_type = long long;

struct user {
   // We will add new friends to a user much often than remove them,
   // therefore I will store them in a vector where insertion in the
   // end is O(1) and traversal is best. Removing a friend will const
   // O(n). Another option would be std::set but I want to avoid linked
   // data structures for now.
   std::vector<uid_type> friends;

   // Groups owned by this user.
   std::vector<gid_type> groups_owner;
};

struct group {
   uid_type owner {-1};
   std::vector<uid_type> members;
};

struct data {
   std::unordered_map<uid_type, user> users;

   // Gropus will be added in the groups array and wont be removed,
   // instead, we will set its owner to -1 and push its index in the
   // vector. When a new group is requested we will pop one index
   // from the stack and use it, if none is available we push_back in
   // the vector.
   std::stack<gid_type> avail_groups_idxs;
   std::vector<group> groups;

   void add_user(uid_type id, user u)
   {
      // Consider insert_or_assign and also std::move.
      auto new_user = users.insert({id, u});
      if (!new_user.second) {
         // The user already exists. This case can be triggered by
         // two conditions: (1) The user inserted or removed a
         // contact from his phone and is sending us his new
         // contacts. (2) The user lost his phone or simply changed
         // his number and the number was assigned to a new person.
         // REVIEW: For now we will simply override previous values
         // and decide later how to handle (2) properly.

         // REVIEW: Is it correct to assume that if the insertion
         // failed than the user object remained unmoved? 
         new_user.first->second.friends = std::move(u.friends);
         return; }

      // Insertion took place and now we have to test which of its
      // friends are already and add them as user friends.
      for (auto const& o : u.friends) { auto existing_user =
         users.find(o); if (existing_user == std::end(users))
            continue;

         // A contact was found in our database, let us add him as the
         // user friend.
         new_user.first->second.friends.push_back(o);

         // REVIEW: We also have to inform the existing user that one of
         // his contacts entered in the game. This will be made only upon
         // request, for now, we will only add the new user in the
         // existing user's friends.
         existing_user->second.friends.push_back(new_user.first->first);
      }
   }

   auto alloc_group()
   {
      if (avail_groups_idxs.empty()) {
         auto size = groups.size();
         groups.push_back({});
         return static_cast<gid_type>(size);
      }

      auto i = avail_groups_idxs.top();
      avail_groups_idxs.pop();
      return i;
   }

   void dealloc_group(gid_type gid)
   {
      groups[gid].members = {};
      groups[gid].owner = -1;
      avail_groups_idxs.push(gid);
   }

   auto add_group(group g)
   {
      // REMARK: After creating the groups we have to inform the app
      // what is the group id.

      auto gid = alloc_group();
      groups[gid] = g;

      // Updates the owner with his new group.
      auto match = users.find(g.owner);
      if (match == std::end(users)) {
         // This is a non-existing user. Perhaps the json command was
         // sent with the wrong information signaling a bug in the app.
         return static_cast<gid_type>(-1);
      }

      match->second.groups_owner.push_back(gid);
      return gid;
   }

   group remove_group(gid_type gid)
   {
      if (gid < 0 || gid >= groups.size())
         return {};

      if (groups[gid].owner == -1) {
         // This group is already deallocated. The caller called us
         // with the wrong gid.
         return {};
      }

      // To remove a user we have to inform its members the group
      // has been removed, therefore we will make a copy before
      // removing it.
      auto removed_group = groups[gid];
      dealloc_group(gid);

      // Now we have to remove this gid from the owners list.
      auto user_match = users.find(removed_group.owner);
      if (user_match == std::end(users))
         return {}; // This looks like an internal logic problem.

      auto group_match =
         std::remove( std::begin(user_match->second.groups_owner)
                    , std::end(user_match->second.groups_owner)
                    , removed_group.owner);

      user_match->second.groups_owner.erase( group_match
                               , std::end(user_match->second.groups_owner));
      return removed_group;
   }
};

int main()
{
   std::cout << "sellit" << std::endl;
}


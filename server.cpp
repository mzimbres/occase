#include <iostream>

#include <stack>
#include <vector>
#include <algorithm>
#include <unordered_map>

using uid_type = long long;
using gid_type = long long;

struct user {
   // The operations we are suposed to perform on an user's friends
   // are
   //
   // 1. Insert: Always in the back O(1).
   // 2. Remove: Once in a while we may have to remove a friend. O(n).
   // 3. Search. I do not think we will perfrórm read-only searches.
   //
   // Revove should happens with even less frequency that insertion,
   // what makes this operation non-critical.  Another option would be
   // to use a std::set but I want to avoid linked data structures for
   // now. The number of friends is expected to be no more that 1000.
   // If we begin to perform searche often, we may have to change this
   // to a data structure with faster lookups.
   std::vector<uid_type> friends;

   // The user is expected to create groups, of which he becomes the
   // owner. The total number of groups a user creates won't be high
   // on average, lets us say less than 20. The operations we are
   // expected to perform on the groups a user owns are
   //
   // 1. Insert rarely and always in the back O(1).
   // 2. Remove rarely O(n).
   // 3. Search ??? O(n) (No need now, clarify this).
   // 
   // A vector seems the most suitable for these requirements.
   std::vector<gid_type> own_groups;

   // Removes group owned by this user from his list of groups.
   void remove_owned_group(gid_type group)
   {
      auto group_match =
         std::remove( std::begin(own_groups), std::end(own_groups)
                    , group);

      own_groups.erase(group_match, std::end(own_groups));
   }

   // Further remarks: This user struct will not store the groups it
   // belongs to. This information can be obtained from the groups
   // array in an indirect manner i.e. by traversing it and quering
   // each group whether this user is a member. This is a very
   // expensive operation and I am usure we need it. However, the app
   // will have to store the groups it belongs to so that it can send
   // messages to to group.
};

struct group {
   uid_type owner {-1}; // The user that owns this group.

   // The number of members in a group is expected be be on the
   // thousends, let us say 100k. The operations performed are
   //
   // 1. Insert: Quite often on the back resulting in O(1).
   // 2. Remove: Once in a while
   // 3. Search: I am not sure yet, but for security reasons we may
   //            have to always check if the user has right to
   //            announce in the group in case the app sends us an
   //            incorect old group id.
   // 
   // For now I will stick with a vector that is very bad if 3. is
   // true and change it later if the need arises.
   std::vector<uid_type> members;
};

struct server_data {
   // May grow up to millions of users.
   std::unordered_map<uid_type, user> users;

   // Groups will be added in the groups array and wont be removed,
   // instead, we will set its owner to -1 and push its index in the
   // stack. When a new group is requested we will pop one index from
   // the stack and use it, if none is available we push_back in the
   // vector.
   std::stack<gid_type> avail_groups_idxs;
   std::vector<group> groups;

   void add_user(uid_type id, user u)
   {
      // Consider insert_or_assign and also std::move.
      auto new_user = users.insert({id, u});
      if (!new_user.second) {
         // The user already exists. This case can be triggered by
         // some conditions
         // 1. The user inserted or removed a contact from his phone
         //    and is sending us his new contacts.
         // 2. The user lost his phone and the number was assigned to
         //    a new person.
         // REVIEW: For now we will simply override previous values
         // and decide later how to handle (2) properly.

         // REVIEW: Is it correct to assume that if the insertion
         // failed than the user object remained unmoved? 
         new_user.first->second.friends = std::move(u.friends);
         return;
      }

      // Insertion took place and now we have to test which of its
      // friends are already registered and add them as the user
      // friends.
      for (auto const& o : u.friends) {
         auto existing_user = users.find(o);
         if (existing_user == std::end(users))
            continue;

         // A contact was found in our database, let us add him as a
         // user friend. We do not have to check if it is already in
         // the array as case would handled by the if above.
         new_user.first->second.friends.push_back(o);

         // REVIEW: We also have to inform the existing user that one of
         // his contacts registered. This will be made only upon
         // request, for now, we will only add the new user in the
         // existing user's friends and him be notified if the new
         // user sends him a message.
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
      // what group id was assigned.
      auto gid = alloc_group();
      groups[gid] = g;

      // Updates the owner with his new group.
      auto match = users.find(g.owner);
      if (match == std::end(users)) {
         // This is a non-existing user. Perhaps the json command was
         // sent with the wrong information signaling a logic error in
         // the app.
         return static_cast<gid_type>(-1);
      }

      match->second.own_groups.push_back(gid);
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
      // removing and return it.
      auto removed_group = groups[gid];
      dealloc_group(gid);

      // Now we have to remove this gid from the owners list.
      auto user_match = users.find(removed_group.owner);
      if (user_match == std::end(users))
         return {}; // This looks like an internal logic problem.

      user_match->second.remove_owned_group(gid);
      return removed_group;
   }
};

int main()
{
   std::cout << "sellit" << std::endl;
   server_data sd;
}


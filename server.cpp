#include <iostream>

#include <set>
#include <stack>
#include <chrono>
#include <vector>
#include <algorithm>
#include <unordered_map>
#include <unordered_set>

using uid_type = long long;
using gid_type = long long;

class user {
private:
   // The operations we are suposed to perform on an user's friends
   // are
   //
   // 1. Insert: On registration and as user adds or remove friends.
   // 2. Remove: On registration and as user adds or remove friends.
   // 3. Search. I do not think we will perform read-only searches.
   //
   // Revove should happens with even less frequency that insertion,
   // what makes this operation non-critical. The number of friends is
   // expected to be no more that 1000.  If we begin to perform
   // searche often, we may have to change this to a data structure
   // with faster lookups.
   std::set<uid_type> friends;

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

public:
   void add_friend(uid_type uid)
   {
      friends.insert(uid);
   }

   void remove_friend(uid_type uid)
   {
      friends.erase(uid);
   }

   // Removes group owned by this user from his list of groups.
   void remove_group(gid_type group)
   {
      auto group_match =
         std::remove( std::begin(own_groups), std::end(own_groups)
                    , group);

      own_groups.erase(group_match, std::end(own_groups));
   }

   void add_group(gid_type gid)
   {
      own_groups.push_back(gid);
   }

   // Further remarks: This user struct will not store the groups it
   // belongs to. This information can be obtained from the groups
   // array in an indirect manner i.e. by traversing it and quering
   // each group whether this user is a member. This is a very
   // expensive operation and I am usure we need it. However, the app
   // will have to store the groups it belongs to so that it can send
   // messages to to group.
};

enum class group_membership
{
   // Member is allowed to read posts and has no access to poster
   // contact. That means in practice he cannot buy anything but only
   // see the traffic in the group and decide whether it is worth to
   // updgrade.
   WATCH

   // Allowed to see posts, see poster contact and post. He gets posts
   // with delay of some hours and his posts are subject to whether he
   // has provided the bank details.
,  DEFAULT
   
   // Like DEFAULT but get posts without any delay.
,  PREMIUM
};

struct group_mem_info {
   group_membership membership {group_membership::DEFAULT};
   std::chrono::seconds delay {0};
};

enum class group_visibility
{
   // Members do not need auhorization to enter the group.
   PUBLIC

   // Only the owner can add members.
,  PRIVATE
};

class group {
private:
   group_visibility visibility {group_visibility::PUBLIC};
   
   uid_type owner {-1}; // The user that owns this group.

   // The number of members in a group is expected be on the
   // thousands, let us say 100k. The operations performed are
   //
   // 1. Insert: Quite often. To avoid inserting twice we have search
   //            before inserting.
   // 2. Remove: Once in a while, also requires searching.
   // 3. Search: I am not sure yet, but for security reasons we may
   //            have to always check if the user has the right to
   //            announce in the group in case the app sends us an
   //            incorrect old group id.
   // 
   // Given those requirements above, I think a hash table is more
   // appropriate.
   std::unordered_map< uid_type
                     , group_mem_info> members;

public:
   auto get_owner() const noexcept {return owner;}
   void set_owner(uid_type uid) noexcept {owner = uid;}

   auto is_owned_by(uid_type uid) const noexcept
   {
      return uid > 0 && owner == uid;
   }

   void reset()
   {
      owner = -1;
      members = {};
   }

   void add_member(uid_type uid)
   {
      members.insert({uid, {}});
   }

   void remove_member(uid_type uid)
   {
      members.erase(uid);
   }
};

// Items will be added in the vector and wont be removed, instead,
// we will push its index in the stack. When a new group is
// requested we will pop one index from the stack and use it, if
// none is available we push_back in the vector.
template <class T>
class grow_only_vector {
public:
   using size_type = typename std::vector<T>::size_type;
   using reference = typename std::vector<T>::reference;
   using const_reference = typename std::vector<T>::const_reference;

private:
   std::stack<size_type> avail;
   std::vector<T> items;

public:
   reference operator[](size_type i)
   { return items[i]; };

   const_reference operator[](size_type i) const
   { return items[i]; };

   // Returns the index of an element int the group that is free
   // for use.
   auto allocate()
   {
      if (avail.empty()) {
         auto size = items.size();
         items.push_back({});
         return size;
      }

      auto i = avail.top();
      avail.pop();
      return i;
   }

   void deallocate(size_type idx)
   {
      avail.push(idx);
   }

   auto is_valid_index(size_type idx) const noexcept
   {
      return idx >= 0 && idx < items.size();
   }
};

class server_data {
private:
   // May grow up to millions of users.
   std::unordered_map<uid_type, user> users;

   grow_only_vector<group> groups;

public:
   void add_user(uid_type id, std::vector<uid_type> contacts)
   {
      auto new_user = users.insert({id, {}});
      if (!new_user.second) {
         // The user already exists. This case can be triggered by
         // some conditions
         // 1. The user inserted or removed a contact from his phone
         //    and is sending us his new contacts.
         // 2. The user lost his phone and the number was assigned to
         //    a new person.
         // REVIEW: For now we will handle 1. and think about 2.
         // later.
         //
         // Some of the user contacts are not registered, that means
         // we have to check one for one. But since this is not any
         // different that in the case the user did not exist we can
         // continue.
      }

      // Regardless whether insertion took place or not, we have to
      // test which of its friends are already registered and add them
      // as a user friend one by one.
      for (auto const& o : contacts) {
         auto existing_user = users.find(o);
         if (existing_user == std::end(users))
            continue;

         // A contact was found in our database, let us add him as a
         // user friend. We do not have to check if it is already in
         // the array as case would handled by the set automaticaly.
         new_user.first->second.add_friend(o);

         // REVIEW: We also have to inform the existing user that one of
         // his contacts just registered. This will be made only upon
         // request, for now, we will only add the new user in the
         // existing user's friends and let him be notified if the new
         // user sends him a message.
         existing_user->second.add_friend(new_user.first->first);
      }
   }

   auto add_group(uid_type owner)
   {
      // Before allocating a new group it is a good idea to check if
      // the owner passed to us indeed exists.
      auto match = users.find(owner);
      if (match == std::end(users)) {
         // This is a non-existing user. Perhaps the json command was
         // sent with the wrong information signaling a logic error in
         // the app.
         return static_cast<gid_type>(-1);
      }

      // Updates the user with his new group.
      match->second.add_group(owner);

      // We can proceed and allocate the group.
      auto gid = groups.allocate();
      groups[gid].set_owner(owner);

      return owner;
   }

   group remove_group(uid_type owner, gid_type gid)
   {
      if (!groups.is_valid_index(gid))
         return {}; // Out of range? Logic error.

      // The user must be the owner of the group to be allowed to
      // remove it
      if (!groups[gid].is_owned_by(owner))
         return {}; // Sorry, you are not allowed.

      // To remove a group we have to inform its members the group has
      // been removed, therefore we will make a copy before removing
      // and return it.
      auto removed_group = groups[gid];
      groups[gid].reset();
      groups.deallocate(gid);

      // Now we have to remove this gid from the owners list.
      auto user_match = users.find(removed_group.get_owner());
      if (user_match == std::end(users))
         return {}; // This looks like an internal logic problem.

      user_match->second.remove_group(gid);
      return removed_group;
   }

   auto change_group_ownership( uid_type from, uid_type to
                              , gid_type gid)
   {
      if (!groups.is_valid_index(gid))
         return false;

      if (!groups[gid].is_owned_by(from))
         return false; // Sorry, you are not allowed.

      auto from_match = users.find(from);
      if (from_match == std::end(users))
         return false;

      auto to_match = users.find(to);
      if (to_match == std::end(users))
         return false;

      // The new owner exists.
      groups[gid].set_owner(to);
      to_match->second.add_group(gid);
      from_match->second.remove_group(gid);
      return true;
   }

   void add_group_member( uid_type owner
                        , uid_type new_member
                        , gid_type gid)
   {
      if (!groups.is_valid_index(gid))
         return;

      if (!groups[gid].is_owned_by(owner))
         return;
         
      auto n = users.count(new_member);
      if (n == 0)
         return; // The user does not exist.

      groups[gid].add_member(new_member);
   }
};

int main()
{
   std::cout << "sellit" << std::endl;
   server_data sd;
}


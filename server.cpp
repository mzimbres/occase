#include <iostream>

#include <set>
#include <stack>
#include <chrono>
#include <vector>
#include <algorithm>
#include <unordered_map>
#include <unordered_set>

// Internally useres and groups will be refered to by their index on a
// vector.
using index_type = int;

// This is the type we will use to associate a user  with something
// that identifies them in the real world like their phone numbers or
// email.
using id_type = int;

// Items will be added in the vector and wont be removed, instead, we
// will push its index in the stack. When a new item is requested we
// will pop one index from the stack and use it, if none is available
// we push_back in the vector.
template <class T>
class grow_only_vector {
public:
   // I do not want an unsigned index type.
   //using size_type = typename std::vector<T>::size_type;
   using size_type = int;
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
         return static_cast<size_type>(size);
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
      return idx >= 0
          && idx < static_cast<size_type>(items.size());
   }
};

class user {
private:
   // The operations we are suposed to perform on a user's friends
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
   std::set<index_type> friends;

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
   std::vector<index_type> own_groups;

public:
   void add_friend(index_type uid)
   {
      friends.insert(uid);
   }

   void remove_friend(index_type uid)
   {
      friends.erase(uid);
   }

   // Removes group owned by this user from his list of groups.
   void remove_group(index_type group)
   {
      auto group_match =
         std::remove( std::begin(own_groups), std::end(own_groups)
                    , group);

      own_groups.erase(group_match, std::end(own_groups));
   }

   void add_group(index_type gid)
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
   
   index_type owner {-1}; // The user that owns this group.

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
   std::unordered_map< index_type
                     , group_mem_info> members;

public:
   auto get_owner() const noexcept {return owner;}
   void set_owner(index_type idx) noexcept {owner = idx;}

   auto is_owned_by(index_type uid) const noexcept
   {
      return uid > 0 && owner == uid;
   }

   void reset()
   {
      owner = -1;
      members = {};
   }

   void add_member(index_type uid)
   {
      members.insert({uid, {}});
   }

   void remove_member(index_type uid)
   {
      members.erase(uid);
   }
};

class server_data {
private:
   grow_only_vector<group> groups;
   grow_only_vector<user> users;

   // May grow up to millions of users.
   std::unordered_map<id_type, index_type> id_to_idx_map;

public:

   // This function is is used to add a user say when he first
   // installs the app and it sends the first message to the server.
   // It will basically allocate his tables internally.
   //
   // id:       User telephone.
   // contacts: Telephones of his contacts. 
   // return:   Index in the users vector that we will use to refer to
   //           him without having to perform searches. This is what
   //           we will return to the user to be stored in his app.
   auto add_user(id_type id, std::vector<index_type> contacts)
   {
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
      }

      // The user did not exist in the system. We have to allocate
      // space for him.
      auto new_user_idx = users.allocate();
      
      // Now we can add all his cantact that are already registered in
      // the app.
      for (auto const& o : contacts) {
         auto existing_user = id_to_idx_map.find(o);
         if (existing_user == std::end(id_to_idx_map))
            continue;

         // A contact was found in our database, let us add him as a
         // the new user friend. We do not have to check if it is
         // already in the array as case would handled by the set
         // automaticaly.
         auto idx = existing_user->second;
         users[new_user_idx].add_friend(idx);

         // REVIEW: We also have to inform the existing user that one of
         // his contacts just registered. This will be made only upon
         // request, for now, we will only add the new user in the
         // existing user's friends and let him be notified if the new
         // user sends him a message.
         users[idx].add_friend(new_user_idx);
      }

      return new_user_idx;
   }

   // TODO: update_user_contacts.

   // Adds new group for the specified owner and returns its index.
   auto add_group(index_type owner)
   {
      // Before allocating a new group it is a good idea to check if
      // the owner passed is at least in a valid range.
      if (users.is_valid_index(owner)) {
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

   // Removes the group and updates the owner.
   auto remove_group(index_type idx)
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

   auto change_group_ownership( index_type from, index_type to
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

   auto add_group_member( index_type owner, index_type new_member
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
   }
};

int main()
{
   std::cout << "sellit" << std::endl;
   server_data sd;
}


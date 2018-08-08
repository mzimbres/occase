#pragma once

#include <chrono>
#include <unordered_map>

#include "config.hpp"

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


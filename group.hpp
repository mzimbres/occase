#pragma once

#include <chrono>
#include <unordered_map>

#include "config.hpp"
#include "user.hpp"
#include "json_utils.hpp"
#include "grow_only_vector.hpp"

struct group_mem_info {
   int info; // TODO: Do we need this?
};

class group {
private:
   index_type owner {-1}; // TODO: Review this

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
   group_info info;

public:
   auto get_owner() const noexcept {return owner;}
   void set_owner(index_type idx) noexcept {owner = idx;}
   void set_info(group_info info_) {info = std::move(info_);}
   auto const& get_info() const noexcept {return info;}

   auto is_active() const noexcept {return owner != -1;}

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

   void broadcast_msg( std::string msg
                     , grow_only_vector<user>& users) const;
};


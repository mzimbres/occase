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
   // The number of members in a group is expected be on the
   // thousands, let us say 10k. The operations performed are
   //
   // 1. Insert: Quite often. To avoid inserting twice we have search
   //            before inserting.
   // 2. Remove: Once in a while, also requires searching.
   // 
   // Given those requirements above, I think a hash table is more
   // appropriate.
   std::unordered_map< index_type
                     , group_mem_info> members;
   group_info info;

public:
   void set_info(group_info info_) {info = std::move(info_);}
   auto const& get_info() const noexcept {return info;}

   void reset()
   {
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


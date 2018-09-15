#pragma once

#include <chrono>
#include <memory>
#include <unordered_map>

#include "config.hpp"
#include "user.hpp"
#include "json_utils.hpp"
#include "grow_only_vector.hpp"

class server_session;

class group {
private:
   // The number of members in a group is expected be on the
   // thousands, let us say 10k. The operations performed are
   //
   // 1. Insert: Quite often. To avoid inserting twice we have to
   //            search before inserting.
   // 2. Remove: Once in a while, also requires searching.
   // 
   // Given those requirements above, I think a hash table is more
   // appropriate.
   std::unordered_map< id_type
                     , std::weak_ptr<server_session>> local_members;
   group_info info;

public:
   group(group_info info_) : info({std::move(info_)}) {}
   void set_info(group_info info_) {info = std::move(info_);}
   auto const& get_info() const noexcept {return info;}

   void reset()
   {
      local_members = {};
   }

   void add_member(id_type id, std::shared_ptr<server_session> s)
   {
      local_members.insert({id, s});
   }

   void remove_member(id_type uid)
   {
      local_members.erase(uid);
   }

   void broadcast_msg(std::string msg);
};


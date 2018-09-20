#pragma once

#include <chrono>
#include <memory>
#include <unordered_map>

#include "config.hpp"
#include "user.hpp"
#include "json_utils.hpp"

class server_session;

class group {
private:
   // The number of members in a group is expected be on the
   // thousands, let us say 10k. The operations performed are
   //
   // 1. Remove: Once in a while, also requires searching.
   std::unordered_map< id_type
                     , std::weak_ptr<server_session>> local_members;
public:
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


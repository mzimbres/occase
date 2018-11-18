#pragma once

#include <string>
#include <memory>
#include <unordered_map>

namespace rt
{

class server_session;

class channel {
private:
   // The number of members in a channel is expected be on the
   // thousands, let us say 50k. The operations performed are
   //
   // 1. Insert very often. Can be on the back.
   // 2. Traverse very often.
   // 3. Remove: Once in a while, requires searching.
   // 
   // Would like to use a vector but cannot pay for the linear search
   // time, even if not occurring very often.
   std::unordered_map< std::string
                     , std::weak_ptr<server_session>
                     > members;

public:
   void broadcast(std::string const& msg);
   void add_member(std::shared_ptr<server_session> s);
};

}


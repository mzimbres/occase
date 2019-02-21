#pragma once

#include <string>
#include <memory>
#include <vector>

namespace rt
{

class proxy_session;

// It is possible to support remove later by keeping a sorted list of
// user_id that we seach as we traverse the vector when publishing the
// messages. No need for that at the moment.

class channel {
private:
   // The number of members in a channel is expected be on the
   // thousands, let us say 50k. The operations performed are
   //
   // 1. Insert very often. Can be on the back.
   // 2. Traverse very often.
   // 3. Remove will be handler by expired weak_ptr.
   std::vector<std::weak_ptr<proxy_session>> members;

public:
   void broadcast(std::string const& msg);
   void add_member(std::weak_ptr<proxy_session> s);
};

}


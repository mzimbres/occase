#include "channel.hpp"

#include "server_session.hpp"

namespace rt
{

void channel::broadcast(std::string const& msg)
{
   //std::cout << "Broadcast size: " << std::size(members) << std::endl;
   auto end = std::size(members);
   unsigned begin = 0;
   while (begin != end) {
      if (auto s = members[begin].lock()) {
         // The user is online. Send him a message and continue.
         if (auto ss = s->session.lock()) {
            ss->send(msg);
         } else {
            assert(false);
         }
         ++begin;
         continue;
      }

      // The user is offline. We can move its expired session to the
      // end of the vector to pop it later.
      --end;

      // Do we need this check?
      if (begin != end)
         std::swap(members[begin], members[end]);
   }
   
   members.resize(begin + 1);
}

void channel::add_member(std::weak_ptr<proxy_session> s)
{
   members.push_back(s);
}

}


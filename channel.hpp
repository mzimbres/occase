#pragma once

#include <string>
#include <memory>
#include <vector>

#include "menu.hpp"
#include "server_session.hpp"

namespace rt
{

/* Operations
 *
 *    The number of members in a channel is expected be on the
 *    thousands, let us say 50k. The operations performed are
 *
 *    1. Insert very often on the back.
 *    2. Traverse on every publication in the channel. Some channels
 *       have high publication rate.
 *    3. Remove not very frequently.
 *
 *    To deal with 3. we introduced the proxy pointers, see
 *    server_session for more details.
 *
 *    It is also possible to support remove later by keeping a sorted
 *    list of user_id that we can search as we traverse the vector
 *    when publishing the messages. No need for that at the moment.
 *
 * Cleanup
 *
 *    Every once in a while we have to clean up expired elements from
 *    the vector. This is due to the fact that there may be users
 *    joining this channel but nobody publishing on it. In this
 *    situation, the weak pointers will never *get out of scope* and
 *    release the memory they refer to.
 *
 * TODO
 *  
 *    - Report channel statistics.
 *    - Since we use a std::vector as the underlying container, the
 *      average memory used will be only half of the memory allocated.
 *      This happens because vectors allocate doubling the current
 *      size.  This is nice to avoid reallocations very often, but
 *      given that we may have hundreds of thousends of vectors. There
 *      will be too much memory wasted. To deal with that we have to
 *      release memory once a stable number of users has been reached,
 *      letting only a small margin for growth. A std::deque could be
 *      also an option but memory would be more fragmented, which is
 *      not good for traversal.
 */

class channel {
private:
   int insertions_on_inactivity = 0;
   std::vector<std::weak_ptr<proxy_session>> members;

   template <class F>
   auto cleanup_traversal(F f)
   {
      auto end = std::size(members);
      decltype(end) begin = 0;
      while (begin != end) {
         if (auto s = members[begin].lock()) {
            // The user is online. Send him a message and continue.
            if (auto ss = s->session.lock()) {
               f(ss);
            } else {
               assert(false);
            }
            ++begin;
            continue;
         }

         // The user is offline. We can move its expired session to the
         // end of the vector to pop it later.
         --end;

         if (begin != end)
            std::swap(members[begin], members[end]);
      }
      
      auto const n = std::size(members);
      members.resize(begin + 1);
      return n - begin + 1;
   }

public:
   void broadcast(std::string const& msg)
   {
      auto const f = [&](auto session)
      { session->send(msg); };

      // TODO: Use the return value for statistics.
      cleanup_traversal(f);
      insertions_on_inactivity = 0;
   }

   void add_member( std::weak_ptr<proxy_session> s
                  , int cleanup_freq)
   {
      members.push_back(s);

      if (++insertions_on_inactivity == cleanup_freq) {
         cleanup_traversal([](auto){});
         insertions_on_inactivity = 0;
      }
   }
};

}


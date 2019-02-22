#pragma once

#include <string>
#include <memory>
#include <vector>

#include "server_session.hpp"

namespace rt
{

/*
 * The number of members in a channel is expected be on the thousands,
 * let us say 50k. The operations performed are
 *
 * 1. Insert very often on the back.
 * 2. Traverse on every publication in the channel.
 * 3. Remove not very frequently.
 *
 * To deal with 3. we introduced the proxy pointers, see
 * server_session for more details.
 *
 * It is possible to support remove later by keeping a sorted list of
 * user_id that we can search as we traverse the vector when
 * publishing the messages. No need for that at the moment.
 */

class channel {
private:
   // Every once in a while we have to clean up expired elements from
   // the vector. This is due to the fact that there may be users
   // joining this channel but nobody publishing on it. In this
   // situation, the weak pointers will never *get out of scope* and
   // release the memory they refer to. This is the frequency we will
   // be cleaning up the channel if no publish activity is observed.
   int const cleanup_frequency;
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
   channel(int cleanup_frequency_)
   : cleanup_frequency(cleanup_frequency_)
   {}

   void broadcast(std::string const& msg)
   {
      auto const f = [&](auto session)
      { session->send(msg); };

      // TODO: Use the return value for statistics.
      cleanup_traversal(f);
      insertions_on_inactivity = 0;
   }

   void add_member(std::weak_ptr<proxy_session> s)
   {
      members.push_back(s);

      if (++insertions_on_inactivity == cleanup_frequency) {
         cleanup_traversal([](auto){});
         insertions_on_inactivity = 0;
      }
   }
};

}


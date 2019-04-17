#pragma once

#include <string>
#include <memory>
#include <vector>
#include <deque>
#include <algorithm>

#include "menu.hpp"
#include "worker_session.hpp"
#include "json_utils.hpp"
#include "utils.hpp"

namespace rt
{

/* Operations
 *
 *    The number of members in a channel is expected be on the
 *    thousands, let us say 50k. The operations performed are
 *
 *    1. Add sessions very often.
 *    2. Traverse the sessions on every publication in the channel.
 *       Some channels have high publication rate.
 *
 *    To avoid having to remove sessions from the channel we
 *    introduced the proxy pointers, see worker_session for more
 *    details. They basically make it possible to expire all
 *    shared_pointers corresponding to a session at once, leaving us
 *    with an expired std::weak_ptr that will be removed the next time
 *    we traverse the sessions in the channel (which happens on every
 *    publication).
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
 *    release the memory they refer to. The vector with the session
 *    will also grow larger and larger in this situation.
 *
 * THOUGHTS
 *  
 *    - Report channel statistics.
 *    - Since we use a std::vector as the underlying container, the
 *      average memory used will be only half of the memory allocated.
 *      This happens because vectors allocate doubling the current
 *      size.  This is nice to avoid reallocations very often, but
 *      given that we may have hundreds of thousands of vectors. There
 *      will be too much memory wasted. To deal with that we have to
 *      release excess memory once a stable number of users has been
 *      reached, letting only a small margin for growth. A std::deque
 *      could be also an option but memory would be more fragmented,
 *      which is not good for traversal.
 */

// TODO: Consider using a ring buffer to store posts instead of a
// std::deque

struct channel_cfg {
   // The frequency the channel will be cleaned up if no publish
   // activity is observed.
   int cleanup_rate; 

   // Max number of messages stored in the each channel.
   int max_posts; 

   // The maximum number of channels a user is allowed to subscribe
   // to. Remaining channels will be ignored.
   int max_sub; 
};

class channel {
private:
   int insertions_on_inactivity = 0;
   std::vector<std::weak_ptr<proxy_session>> members;

   // For long living servers we need a limit on how big the number of
   // publish items can grow. For that it is more convenient to use a
   // deque.
   std::deque<post> items;

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

   void store_item(post item, int max_posts)
   {
      // We have to ensure this vector stays ordered according to
      // publish ids. Most of the time there will be no problem, but
      // it still may happen that messages are routed to us out of
      // order. Insertion sort is necessary.
      items.push_back(std::move(item));

      auto prev = std::prev(std::end(items));

      // Sorted insertion.
      std::rotate( std::upper_bound(std::begin(items), prev, *prev)
                 , prev
                 , std::end(items));

      // Limits the container size.
      if (ssize(items) > max_posts)
         items.pop_front();
   }

public:
   void broadcast(post item, int max_posts)
   {
      json j_pub;
      j_pub["cmd"] = "publish";
      j_pub["items"] = std::vector<post>{item};

      auto msg = std::make_shared<std::string>(j_pub.dump());
      store_item(std::move(item), max_posts);

      auto f = [msg](auto session)
         { session->send_menu_msg(msg); };

      // TODO: Use the return value for statistics.
      cleanup_traversal(f);
      insertions_on_inactivity = 0;
   }

   void add_member(std::weak_ptr<proxy_session> s, int cleanup_rate)
   {
      members.push_back(s);

      if (++insertions_on_inactivity == cleanup_rate) {
         cleanup_traversal([](auto){});
         insertions_on_inactivity = 0;
      }
   }

   // Copies all items that are newer than id to inserter.
   template <class Inserter>
   void retrieve_pub_items(int id, Inserter inserter) const
   {
      auto const comp = [](auto const& a, auto const& b)
      { return a < b.id; };

      auto const point =
         std::upper_bound( std::begin(items)
                         , std::end(items)
                         , id
                         , comp);

      std::copy(point, std::end(items), inserter);
   }

};

}


#pragma once

#include <deque>
#include <string>
#include <chrono>
#include <memory>
#include <vector>
#include <algorithm>

#include "post.hpp"
#include "utils.hpp"
#include "db_plain_session.hpp"

namespace occase
{

/* Operations
 *
 *    The number of members in a channel is expected be on the
 *    thousands, let us say 50k. The operations performed are
 *
 *    1. Add sessions very often.
 *    2. Traverse the sessions on every publication in the channel.
 *
 *    To avoid having to remove sessions from the channel we
 *    introduced the proxy pointers, see db_session for more
 *    details. They make it possible to expire all weak_pointers
 *    corresponding to a session at once, leaving us with an expired
 *    std::weak_ptr that will be removed the next time we traverse the
 *    sessions in the channel (which happens on every publication).
 *
 *    It is also possible to support remove later by keeping a sorted
 *    list of user_id's that we can search as we traverse the vector
 *    while publishing the messages. No need for that at the moment.
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
 *    Report channel statistics.
 *
 *    Since we use a std::vector as the underlying container, the
 *    average memory used will be only half of the memory allocated.
 *    This happens because vectors allocate doubling the current
 *    size.  This is nice to avoid reallocations very often, but
 *    given that we may have thousands of vectors. There will be too
 *    much memory wasted. To deal with that we have to release
 *    excess memory once a stable number of users has been reached,
 *    letting only a small margin for growth. A std::deque could be
 *    also an option but memory would be more fragmented, which is
 *    not good for traversal.
 */

struct channel_cfg {
   // The frequency the channel will be cleaned up if no publish
   // activity is observed.
   int cleanup_rate; 

   // Time after which the post is considered expired. Input in
   // seconds.
   int post_expiration;

   auto get_post_expiration() const
   {
      return std::chrono::seconds {post_expiration};
   }
};

template <class Session>
class channel {
public:
   using inserter_type = std::back_insert_iterator<std::vector<post>>;

private:
   int insertions_on_inactivity_ = 0;
   std::vector<std::weak_ptr<Session>> members_;
   std::vector<post> items_;

   template <class F>
   auto cleanup_traversal(F f)
   {
      auto end = std::size(members_);
      decltype(end) begin = 0;
      while (begin != end) {
         if (auto s = members_[begin].lock()) {
	    f(s);
            ++begin;
            continue;
         }

         // The user is offline. We can move its expired session to the
         // end of the vector to pop it later.
         --end;

         if (begin != end)
            std::swap(members_[begin], members_[end]);
      }
      
      auto const n = std::size(members_);
      members_.resize(begin + 1);
      return n - begin + 1;
   }

   // Returns the expired posts.
   auto
   remove_expired_posts( std::chrono::seconds now
                       , std::chrono::seconds exp)
   {
      auto f = [this, exp, now](auto const& p)
         { return (p.date + exp) < now; };

      auto point =
         std::partition_point( std::begin(items_)
                             , std::end(items_)
                             , f);

      auto point2 =
         std::rotate( std::begin(items_)
                    , point
                    , std::end(items_));

      auto const n = std::distance(point2, std::end(items_));

      std::vector<post> expired;

      std::copy( point2
               , std::end(items_)
               , std::back_inserter(expired));

      items_.erase(point2, std::end(items_));

      return expired;
   }

   void add_post(post item)
   {
      // We have to ensure this vector stays ordered according to
      // publish ids. Most of the time there will be no problem, but
      // it still may happen that messages are routed to us out of
      // order. Insertion sort is necessary.
      items_.push_back(std::move(item));

      auto prev = std::prev(std::end(items_));

      // Sorted insertion.
      std::rotate( std::upper_bound(std::begin(items_), prev, *prev)
                 , prev
                 , std::end(items_));
   }

public:
   auto
   broadcast( post p
            , std::chrono::seconds now
            , std::chrono::seconds exp)
   {
      json j;
      j["cmd"] = "post";
      j["items"] = std::vector<post>{p};

      auto const msg = std::make_shared<std::string>(j.dump());

      add_post(p);

      auto const expired = remove_expired_posts(now, exp);

      auto f = [msg, &p](auto session)
         { session->send_post(msg, p); };

      cleanup_traversal(f);
      insertions_on_inactivity_ = 0;
      return expired;
   }

   void add_member(std::weak_ptr<Session> s, int cleanup_rate)
   {
      members_.push_back(s);

      if (++insertions_on_inactivity_ == cleanup_rate) {
         cleanup_traversal([](auto){});
         insertions_on_inactivity_ = 0;
      }
   }

   // Copies all items that are newer than id and that satistfies the
   // predicate to inserter.
   template <class UnaryPredicate>
   void
   get_posts( int id
            , inserter_type inserter
            , long int max
            , UnaryPredicate pred) const
   {
      auto comp = [](auto const& a, auto const& b)
         { return a < b.id; };

      auto const point =
         std::upper_bound( std::cbegin(items_)
                         , std::cend(items_)
                         , id
                         , comp);

      // The number of posts that the app did not yet received from
      // this channel.
      auto const n = std::distance(point, std::cend(items_));
      auto const d = std::min(n, max);
      std::copy_if(point, point + d, inserter, pred);
   }

   // Removes a post if it exists in the channel.
   bool remove_post(int id, std::string const& from)
   {
      auto comp = [](auto const& a, auto const& b)
      { return a.id < b; };

      auto const point =
         std::lower_bound( std::begin(items_)
                         , std::end(items_)
                         , id
                         , comp);

      if (point == std::end(items_))
         return false;

      if (point->id != id)
         return false;

      if (point->from != from)
         return false;

      items_.erase(point);
      return true;
   }
};

}


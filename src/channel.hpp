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

struct channel_cfg {
   // The frequency the channel will be cleaned up if no publish
   // activity is observed.
   int cleanup_rate; 

   // Time after which the post is considered expired. Input in
   // seconds.
   int post_expiration;

   auto get_post_expiration() const
      { return std::chrono::seconds {post_expiration}; }
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

public:
   // Returns the expired posts.
   auto
   remove_expired_posts( std::chrono::seconds now
                       , std::chrono::seconds exp)
   {
      // TODO: Implement this with remove_if. We have to traverse all
      // posts as they are not sorted according to date. Consider
      // using boost.MultiIndex. Or some container that allows one to
      // have indices sorted according to many possible criteria.

      //auto f = [this, exp, now](auto const& p)
      //   { return (p.date + exp) < now; };

      //auto point =
      //   std::partition_point( std::begin(items_)
      //                       , std::end(items_)
      //                       , f);

      //auto point2 =
      //   std::rotate( std::begin(items_)
      //              , point
      //              , std::end(items_));

      //auto const n = std::distance(point2, std::end(items_));

      //std::vector<post> expired;

      //std::copy( point2
      //         , std::end(items_)
      //         , std::back_inserter(expired));

      //items_.erase(point2, std::end(items_));

      //return expired;
      return std::vector<post>{};
   }

   void add_post(post item)
   {
      items_.push_back(std::move(item));

      auto prev = std::prev(std::end(items_));

      // Sorted insertion according to the post id.
      auto point =
	 std::upper_bound(std::begin(items_),
	                  prev,
			  *prev,
			  comp_post_id_less {});

      std::rotate(point, prev, std::end(items_));
   }

   void broadcast(post p)
   {
      json j;
      j["cmd"] = "post";
      j["posts"] = std::vector<post>{p};

      auto const msg = std::make_shared<std::string>(j.dump());
      auto f = [msg, &p](auto s)
         { s->send_post(msg, p); };

      cleanup_traversal(f);
      insertions_on_inactivity_ = 0;
   }

   void add_member(std::weak_ptr<Session> s, int cleanup_rate)
   {
      members_.push_back(s);

      if (++insertions_on_inactivity_ == cleanup_rate) {
         cleanup_traversal([](auto){});
         insertions_on_inactivity_ = 0;
      }
   }

   // Copies all items that are newer than date and that satistfies the
   // predicate to inserter.
   template <class UnaryPredicate>
   void
   get_posts( date_type date
            , inserter_type inserter
            , long int max
            , UnaryPredicate pred) const
   {
      auto f = [&](auto const& a, auto const& b)
         { return a < b.date; };

      auto const point =
         std::upper_bound( std::cbegin(items_)
                         , std::cend(items_)
                         , date
                         , f);

      // The number of posts that the app did not yet received from
      // this channel.
      auto const n = std::distance(point, std::cend(items_));
      auto const d = std::min(n, max);
      std::copy_if(point, point + d, inserter, pred);
   }

   // Removes a post if it exists in the channel.
   bool remove_post(std::string const& id, std::string const& from)
   {
      auto f = [&](auto const& p)
         { return p.id == id; };

      auto match = std::find_if(std::begin(items_), std::end(items_), f);

      if (match == std::end(items_))
         return false;

      if (match->id != id)
         return false;

      if (match->from != from)
         return false;

      items_.erase(match);
      return true;
   }

   auto size() const noexcept
      { return std::size(items_); }
};

}


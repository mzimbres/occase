#pragma once

#include <string>
#include <chrono>
#include <vector>
#include <algorithm>

#include "post.hpp"

namespace occase
{

class channel {
private:
   std::vector<post> items_;

public:
   using inserter_type = std::back_insert_iterator<std::vector<post>>;
   struct config {
      // Time after which the post is considered expired. Input in
      // seconds.
      int post_expiration;
      auto get_post_expiration() const noexcept
	 { return std::chrono::seconds {post_expiration}; }
   };

   // Removes and returns expired posts.
   std::vector<post>
   remove_expired_posts(
      std::chrono::seconds now,
      std::chrono::seconds exp);

   void add_post(post item);
   void on_visualizations(std::vector<std::string> const& post_ids);
   void on_click(std::string const& post_id);

   // Removes a post if it exists in the channel.
   bool remove_post(std::string const& id, std::string const& from);
   auto size() const noexcept { return std::size(items_); }

   // Copies all items that are newer than date and that satistfies the
   // predicate to inserter.
   template <class UnaryPredicate>
   void get_posts(
      date_type date,
      inserter_type inserter,
      long int max,
      UnaryPredicate pred) const
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
};

}


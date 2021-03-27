#include "channel.hpp"

#include <string>
#include <chrono>
#include <vector>
#include <algorithm>

#include "post.hpp"

namespace occase
{

std::vector<post>
channel::remove_expired_posts(
   std::chrono::seconds now,
   std::chrono::seconds exp)
{
   // We have to traverse all posts as they are not sorted according
   // to date.

   //auto f = [this, exp, now](auto const& p)
   //   { return (p.date + exp) < now; };

   //auto point =
   //   std::partition_point( std::begin(posts_)
   //                       , std::end(posts_)
   //                       , f);

   //auto point2 =
   //   std::rotate( std::begin(posts_)
   //              , point
   //              , std::end(posts_));

   //auto const n = std::distance(point2, std::end(posts_));

   //std::vector<post> expired;

   //std::copy( point2
   //         , std::end(posts_)
   //         , std::back_inserter(expired));

   //posts_.erase(point2, std::end(posts_));

   //return expired;
   return std::vector<post>{};
}

void channel::add_post(post p)
{
   posts_.push_back(std::move(p));

   auto prev = std::prev(std::end(posts_));

   // Sorted insertion according to the post id.
   auto point =
      std::upper_bound(std::begin(posts_),
		       prev,
		       *prev,
		       comp_post_id_less {});

   std::rotate(point, prev, std::end(posts_));
}

void channel::on_visualization(std::string const& post_id)
{
   auto f = [&post_id](auto const& p)
      { return p.id == post_id; };

   auto match = std::find_if(std::begin(posts_), std::end(posts_), f);
   if (match != std::end(posts_))
      ++match->visualizations;
}

bool channel::remove_post(
   std::string const& id,
   std::string const& from)
{
   auto f = [&](auto const& p)
      { return p.id == id; };

   auto match = std::find_if(std::begin(posts_), std::end(posts_), f);

   if (match == std::end(posts_))
      return false;

   if (match->from != from)
      return false;

   posts_.erase(match);
   return true;
}

std::vector<post> channel::query(post const& p, int max) const
{
   auto const n = static_cast<int>(std::ssize(posts_));
   auto const s = std::min(n, max);
   return std::vector<post>(std::cbegin(posts_), std::cbegin(posts_) + s);
}

void
channel::load_visualizations(
   std::vector<std::pair<std::string, int>> const& v)
{
   auto vbegin = std::cbegin(v);
   auto pbegin = std::begin(posts_);

   while (vbegin != std::cend(v) && pbegin != std::end(posts_)) {
      if (vbegin->first == pbegin->id) {
	 pbegin->visualizations = vbegin->second;
	 ++pbegin;
      }
      ++vbegin;
   }
}

} // occase 

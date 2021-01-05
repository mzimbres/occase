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

void channel::add_post(post item)
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

void channel::on_visualizations(std::vector<std::string> const& post_ids)
{
   auto rbegin = std::rbegin(items_);
   for (auto const id : post_ids) {
      auto f = [id](auto const& p)
	 { return p.id == id; };

      rbegin = std::find_if(rbegin, std::rend(items_), f);
      if (rbegin == std::rend(items_))
	 break;

      ++rbegin->visualizations;
      ++rbegin;
   }
}

void channel::on_click(std::string const& post_id)
{
   auto f = [post_id](auto const& p)
      { return p.id == post_id; };

   auto rbegin = std::find_if(std::rbegin(items_), std::rend(items_), f);
   if (rbegin != std::rend(items_))
      ++rbegin->clicks;
}

bool channel::remove_post(
   std::string const& id,
   std::string const& from)
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

}



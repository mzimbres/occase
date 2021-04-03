#include "channel.hpp"

#include <string>
#include <chrono>
#include <vector>
#include <iterator>
#include <algorithm>

#include "post.hpp"

namespace occase
{

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

std::vector<post>
channel::remove_expired_posts(
   std::chrono::seconds now,
   std::chrono::seconds exp)
{
   // We have to traverse all posts as they are not sorted according
   // to date.

   // Returns true when a post is expired.
   //auto f = [this, exp, now](auto const& p)
   //   { return (p.date + exp) < now; };

   return std::vector<post>{};
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

bool is_child_of(
   std::vector<int> const& code,
   std::vector<int> const& wanted)
{
   if (std::size(wanted) > std::size(code))
      return false;

   auto i = 0U;
   while (i < std::size(wanted) && wanted[i] == code[i])
      ++i;

   return i == std::size(wanted);
}

template <class Receiver>
void filter(post const& p, post const& q, Receiver recv)
{
   if (!is_child_of(p.location, q.location))
      return;

   if (!is_child_of(p.product, q.product))
      return;

   recv(p);
}

std::vector<post> channel::query(post const& q, int max) const
{
   std::vector<post> ret;

   auto f = [&](post const& p)
      { ret.push_back(p); };

   auto g = [&](post const& p)
      { filter(p, q, f); };

   std::for_each(std::cbegin(posts_), std::cend(posts_), g);
   return ret;
}

int channel::count(post const& q) const
{
   int ret;

   auto f = [&](post const&)
      { ++ret; };

   auto g = [&](post const& p)
      { filter(p, q, f); };

   std::for_each(std::cbegin(posts_), std::cend(posts_), g);
   return ret;
}

void channel::load_visualizations(visual_type const& v)
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

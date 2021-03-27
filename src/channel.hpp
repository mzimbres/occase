#pragma once

#include <string>
#include <chrono>
#include <vector>
#include <utility>
#include <algorithm>

#include "post.hpp"

namespace occase {

class channel {
private:
   // The posts sorted by their id.
   std::vector<post> posts_;

public:
   // Adds a new post.
   void add_post(post p);

   // Removes and returns expired posts.
   std::vector<post>
   remove_expired_posts(
      std::chrono::seconds now,
      std::chrono::seconds exp);

   // Increases the number of visualizations of a post by one.
   void on_visualization(std::string const& post_id);

   // Removes a post if it exists in the channel.
   bool remove_post(std::string const& id, std::string const& from);

   // Returns the number of posts.
   auto size() const noexcept { return std::size(posts_); }

   // Returns to posts that satisfy the query.
   std::vector<post> query(post const& p, int max) const;

   // Loads the visualizations in posts. The expected format is
   //
   // {post_id1, n1}, {post_id2, n2} ...
   //
   // that meast the input must have an even number of elements where
   // the keys are a string and the value an integer.
   void load_visualizations(
      std::vector<std::pair<std::string, int>> const & v);
};

}


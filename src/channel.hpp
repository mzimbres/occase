#pragma once

#include <string>
#include <chrono>
#include <vector>
#include <algorithm>

#include "post.hpp"

namespace occase {

class channel {
private:
   // The posts sorted by their id.
   std::vector<post> items_;

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
   auto size() const noexcept { return std::size(items_); }

   // Returns to posts that satisfy the query.
   std::vector<post> query(post const& p, int max) const;
};

}


#pragma once

#include <stack>
#include <deque>
#include <string>
#include <limits>

#include "json_utils.hpp"

namespace rt
{

// Puts all group hashes into a vector.
std::vector<std::string> get_hashes(std::string const& str);

std::string to_str(int i);

struct menu_node {
   std::string name;
   std::string code;
   std::deque<menu_node*> children;
};

class menu {
private:
   menu_node root;
   int max_depth = 0;
public:
   static constexpr auto sep = 3;
   enum class iformat {spaces, counter};
   enum class oformat {spaces, counter, info, hashes};
   menu() = delete;
   menu(menu const&) = delete;
   menu& operator=(menu const&) = delete;
   menu(menu&&) = delete;
   menu& operator=(menu&&) = delete;
   menu(std::string const& str);
   ~menu();

   std::string dump( oformat of
                   , unsigned max_depth =
                      std::numeric_limits<unsigned>::max());
   std::vector<std::string> get_codes_at_depth(unsigned depth) const;
   auto get_max_depth() const noexcept {return max_depth;}
   bool check_leaf_min_depths(unsigned min_depth) const;
};

}


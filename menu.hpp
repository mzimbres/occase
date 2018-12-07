#pragma once

#include <stack>
#include <deque>
#include <string>

#include "json_utils.hpp"

namespace rt
{

json gen_location_menu();
json gen_sim_menu(int l);

// Puts all group hashes into a vector.
std::vector<std::string> get_hashes(json menu);

std::string to_str(int i);

struct menu_node {
   std::string name;
   std::string code;
   std::deque<menu_node*> children;
};

void build_menu_tree(menu_node& root, std::string const& menu_str);

struct menu_leaf_iterator {
   std::stack<std::vector<menu_node*>> st;
   menu_node* current;
   menu_leaf_iterator(menu_node& root);
   void next_leaf();
   void next();
   void advance();
   bool end() const noexcept { return std::empty(st); }
};

}


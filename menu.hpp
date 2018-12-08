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
   std::deque<menu_node*> children;
};

class menu {
private:
   menu_node root;
public:
   menu(std::string const& str);
   ~menu();
   void print_leaf();
   void print_all();
};

}


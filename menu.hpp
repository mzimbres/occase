#pragma once

#include <stack>
#include <deque>
#include <string>

#include "json_utils.hpp"

namespace rt
{

json gen_location_menu();
std::string gen_sim_menu(int l);

// Puts all group hashes into a vector.
std::vector<std::string> get_hashes(json menu);

std::string to_str(int i);

struct menu_node {
   std::string name;
   std::string code;
   std::deque<menu_node*> children;
};

class menu {
private:
   menu_node root;
public:
   static constexpr auto sep = 3;
   menu() = delete;
   menu(menu const&) = delete;
   menu& operator=(menu const&) = delete;
   menu(menu&&) = delete;
   menu& operator=(menu&&) = delete;
   menu(std::string const& str);
   ~menu();

   void dump(int type, bool hash);
   std::vector<std::string> get_leaf_codes() const;
   void print_leaf();
};

}


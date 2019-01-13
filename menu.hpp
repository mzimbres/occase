#pragma once

#include <stack>
#include <deque>
#include <string>
#include <limits>

#include "json_utils.hpp"

namespace rt
{

// Puts all group hashes into a vector.
std::vector<std::string>
get_hashes(std::string const& str, unsigned depth);

std::string to_str(int i);

struct menu_node {
   std::string name;
   std::string code;
   unsigned leaf_counter = 0;
   std::deque<menu_node*> children;
};

/*  If the input file uses spaces to show the depth, then the line
 *  separator will be assumed to be '\n'. Furthermore it will also
 *  assume that the leaf counter is not present. 
 *
 *  If the input file uses a counter to show the depth, then the field
 *  separator will be assumed to be ';' and the line separator '='.
 *
 *  It is still possible to output with counter using line separator
 *  '\n' but in this case the output cannot be read back by the menu.
 */
class menu {
private:
   menu_node root;
   int max_depth = 0;

   template <int>
   friend class menu_view;
public:
   static constexpr auto sep = 3;
   static constexpr unsigned max_supported_depth = 10;
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
                   , char line_sep = '\n'
                   , unsigned max_depth = max_supported_depth);
   auto get_max_depth() const noexcept {return max_depth;}
   auto empty() const
   {
      return std::empty(root.children);
   }
};

bool check_leaf_min_depths(menu& m, unsigned min_depth);

class menu_traversal {
private:
   std::deque<std::deque<menu_node*>> st;
   unsigned depth;

public:
   menu_traversal(menu_node* root, unsigned depth_);
   menu_node* advance_to_leaf();
   menu_node* next_internal();
   menu_node* next_leaf_node();
   menu_node* next_node();
   auto get_depth() const noexcept { return std::size(st); }
};

template <int N>
class leaf_iterator {
private:
   menu_traversal iter;
   menu_node* current = nullptr;

public:
   using value_type = menu_node;
   using difference_type = std::ptrdiff_t;
   using reference = menu_node&;
   using const_reference = menu_node const&;
   using pointer = menu_node*;
   using const_pointer = menu_node const*;
   using iterator_category = std::forward_iterator_tag;

   leaf_iterator( menu_node* root = nullptr
                , unsigned depth = menu::max_supported_depth)
   : iter {root, depth}
   { 
      if (root)
         current = iter.advance_to_leaf();
   }

   reference operator*() { return *current;}
   const_reference const& operator*() const { return *current;}

   leaf_iterator& operator++()
   {
      if constexpr (N == 0)
         current = iter.next_leaf_node();
      else
         current = iter.next_node();

      return *this;
   }

   leaf_iterator operator++(int)
   {
      leaf_iterator ret(*this);
      ++(*this);
      return ret;
   }

   pointer operator->() {return current;}
   const_pointer operator->() const {return current;}

   friend auto operator==(leaf_iterator const& a, leaf_iterator const& b)
   {
      return a.current == b.current;
   }

   friend auto operator!=(leaf_iterator const& a, leaf_iterator const& b)
   {
      return !(a == b);
   }

   // Extensions to the common iterator interface.
   auto get_depth() const noexcept { return iter.get_depth(); }
   auto* get_pointer_to_node() const {return current;}
};

template <int N>
class menu_view {
private:
   unsigned depth;
   menu_node* root = nullptr;

public:
   using iterator = leaf_iterator<N>;

   menu_view( menu_node* root_
            , unsigned depth_ = menu::max_supported_depth)
   : depth {depth_}
   , root {root_}
   { }

   menu_view( menu& m
            , unsigned depth_ = menu::max_supported_depth)
   : menu_view {m.root.children.front(), depth_}
   { }

   iterator begin() const {return iterator{root, depth};}
   iterator end() const {return iterator{};}
};

struct menu_elem {
   std::string data;
   unsigned depth = 0;
   int version = 0;
};

void to_json(json& j, menu_elem const& e);
void from_json(json const& j, menu_elem& e);

std::vector<std::string>
comb_hash_codes(std::vector<std::vector<std::string>> const& codes);

std::vector<std::string>
menu_elems_to_comb_hash_codes(std::vector<menu_elem> const& elems);

std::vector<int>
read_versions(std::vector<menu_elem> const& elems);

}


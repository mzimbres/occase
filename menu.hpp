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

class menu {
private:
   menu_node root;
   int max_depth = 0;

   template <int>
   friend class menu_view;
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
   auto get_max_depth() const noexcept {return max_depth;}
   bool check_leaf_min_depths(unsigned min_depth) const;
   auto empty() const
   {
      return std::empty(root.children);
   }
};

class menu_traversal {
private:
   std::deque<std::deque<menu_node*>> st;
   unsigned depth;

   void advance();
   void next_internal();

public:
   menu_node* current;
   menu_traversal( menu_node* root
                , unsigned depth_ = std::numeric_limits<unsigned>::max());
   auto get_depth() const noexcept { return std::size(st); }
   void next_leaf_node();
   void next_node();
   bool end() const noexcept { return std::empty(st); }
};

template <int N>
class leaf_iterator {
private:
   menu_traversal iter;

public:
   using value_type = menu_node;
   using difference_type = std::ptrdiff_t;
   using reference = menu_node&;
   using pointer = menu_node*;
   using const_pointer = menu_node const*;
   using iterator_category = std::forward_iterator_tag;

   leaf_iterator( menu_node* node = nullptr
                , unsigned depth = std::numeric_limits<unsigned>::max())
   : iter {node, depth}
   { }

   menu_node& operator*() { return *iter.current;}
   menu_node const& operator*() const { return *iter.current;}

   leaf_iterator& operator++()
   {
      if constexpr (N == 0)
         iter.next_leaf_node();
      else
         iter.next_node();

      return *this;
   }

   leaf_iterator operator++(int)
   {
      leaf_iterator ret(*this);
      ++(*this);
      return ret;
   }

   menu_node* operator->() {return iter.current;}
   menu_node const* operator->() const {return iter.current;}

   friend auto operator==(leaf_iterator const& a, leaf_iterator const& b)
   {
      return a.iter.current == b.iter.current;
   }

   friend auto operator!=(leaf_iterator const& a, leaf_iterator const& b)
   {
      return !(a == b);
   }
};

template <int N>
class menu_view {
private:
   unsigned depth;
   menu_node* root = nullptr;

public:
   using iterator = leaf_iterator<N>;

   menu_view( menu& m
            , unsigned depth_ = std::numeric_limits<unsigned>::max())
   : depth {depth_}
   , root {m.root.children.front()}
   { }

   iterator begin() const {return iterator{root, depth};}
   iterator end() const {return iterator{};}
};

}


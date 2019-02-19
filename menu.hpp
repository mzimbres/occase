#pragma once

#include <stack>
#include <deque>
#include <vector>
#include <string>
#include <limits>
#include <cstdint>

#include "json_utils.hpp"

namespace rt
{

std::string to_str_raw(int i, int width, char fill);
std::string to_str(int i);

struct menu_node {
   std::string name;
   std::vector<int> code;
   int leaf_counter = 0;
   std::deque<menu_node*> children;
};

template <class Iter>
std::string get_code_as_str_impl(Iter begin, Iter end)
{
   if (begin == end)
      return {};

   if (std::distance(begin, end) == 1)
      return to_str_raw(*begin, 3, '0');

   std::string code;
   for (; begin != std::prev(end); ++begin)
      code += to_str_raw(*begin, 3, '0') + ".";

   code += to_str_raw(*begin, 3, '0');
   return code;
}

inline
std::string get_code_as_str(std::vector<int> const& v)
{
   return get_code_as_str_impl(std::begin(v), std::end(v));
}

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
   menu_node head;
   int max_depth = -1;

   template <int>
   friend class menu_view;

public:
   static constexpr auto sep = 3;
   static constexpr auto max_supported_depth = 10;
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
                   , int max_depth = max_supported_depth);

   auto get_max_depth() const noexcept
      { return max_depth; }

   auto empty() const noexcept
      { return std::empty(head.children); }
};

bool check_leaf_min_depths(menu& m, int min_depth);

class menu_traversal {
private:
   std::deque<std::deque<menu_node*>> st;
   int depth;

public:
   menu_traversal(menu_node* root, int depth_);
   menu_node* advance_to_leaf();
   menu_node* next_internal();
   menu_node* next_leaf_node();
   menu_node* next_node();
   auto get_depth() const noexcept
      { return static_cast<int>(std::size(st)) - 1; }
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
                , int depth = menu::max_supported_depth)
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

   pointer operator->()
      {return current;}

   const_pointer operator->() const
      {return current;}

   friend
   auto operator==(leaf_iterator const& a, leaf_iterator const& b)
      { return a.current == b.current; }

   friend
   auto operator!=(leaf_iterator const& a, leaf_iterator const& b)
      { return !(a == b); }

   // Extensions to the common iterator interface.
   auto get_depth() const noexcept
      { return iter.get_depth(); }

   auto* get_pointer_to_node() const
      { return current; }
};

template <int N>
class menu_view {
private:
   int depth;
   menu_node* root = nullptr;

public:
   using iterator = leaf_iterator<N>;

   menu_view(menu_node* root_, int depth_ = menu::max_supported_depth)
   : depth {depth_}
   , root {root_}
   { }

   menu_view(menu& m, int depth_ = menu::max_supported_depth)
   : menu_view {m.head.children.front(), depth_}
   { }

   iterator begin() const {return iterator{root, depth};}
   iterator end() const {return iterator{};}
};

struct menu_elem {
   std::string data;
   int depth = 0;
   int version = 0;
};

void to_json(json& j, menu_elem const& e);
void from_json(json const& j, menu_elem& e);

/* This function receives input in the form
 *
 *   _________menu_1__________     __________menu_2________   etc.
 *  |                         |   |                        |
 * [[[1, 2, 3], [3, 4, 1], ...], [[a, b, c], [d, e, f], ...], ...]
 *
 * The menu_1 may refer to regions of a country and menu_2 to a
 * product for example.  The inner most array contains the coordinates
 * of the menu items the user wants to subscribe to. It has the form
 *
 *    [1, 2, 3, ..]
 *
 * Each position in this array refers to a level in the menu, for
 * example
 *
 *    [State, City, Neighbourhood]
 *
 * This array is contained in another array with the other channels
 * the user wants to subscribe to, for example
 *
 *    [Sao Paulo, Atibaia, Vila Santista]
 *    [Sao Paulo, Atibaia, Centro]
 *    [Sao Paulo, Compinas, Barao Geraldo]
 *    ...
 *
 * These arrays in turn are grouped in the outer most array where each
 * element corresponds to a menu. There will be typically two or three
 * menus per app.
 * 
 * The function respects the menu depth, so if the menu coodinates
 * have length 6 but the the hash codes are generate for depth 2 only
 * the first two elements will be considered.
 *
 * The output is the combination of the codes respecting the depths.
 * For the input array above the output would be
 *
 * [[[[1, 2], [a, b]]], [[[1, 2], [c, d]]], ..., [[[3, 4], [a, b]]], ...
 *
 * Each element of the outermost array will have length one.
 */
std::vector<std::vector<std::vector<std::vector<int>>>>
channel_codes( std::vector<std::vector<std::vector<int>>> const& codes
             , std::vector<menu_elem> const& menu_elems);

/* This function will hash a channel code in the form
 *
 *   __menu_1__    __menu_2__    __menu_3__         
 *  |          |  |          |  |          |  
 * [[[1, 3,  4]], [[5, 1,  4]], [[2, 1,  8]]]
 *
 * to an 64 bits integer which will be used for hashing. Each menu
 * configuration requires its own hashing function. This version is
 * suitable for two menus where both have filter depth 2.
 *
 * TODO: Move this function to some other header.
 * TODO: Check code validity on every call to this function.
 *
 */
std::uint64_t convert_to_channel_code(
      std::vector<std::vector<std::vector<int>>> const& codes);

/* This function will convert a vector of menu_elem in the structure
 * that is input for channel_codes decribed above.
 */
std::vector<std::vector<std::vector<int>>>
menu_elems_to_codes(std::vector<menu_elem> const& elems);

std::vector<int>
read_versions(std::vector<menu_elem> const& elems);

}


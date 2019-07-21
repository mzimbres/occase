#pragma once

#include <stack>
#include <deque>
#include <vector>
#include <string>
#include <limits>
#include <cstdint>

#include "utils.hpp"
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
   enum class oformat {spaces, counter};
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

template <int N>
class menu_traversal {
private:
   std::deque<std::deque<menu_node*>> st;
   int depth;

public:
   menu_traversal(menu_node* root, int depth_)
   : depth(depth_)
   {
      if (root)
         st.push_back({root});
   }

   auto* advance()
   {
      while (!std::empty(st.back().back()->children) &&
            static_cast<int>(std::size(st)) <= depth)
         st.push_back(st.back().back()->children);

      auto* tmp = st.back().back();
      st.back().pop_back();
      return tmp;
   }

   menu_node* next_internal()
   {
      st.pop_back();
      if (std::empty(st))
         return nullptr;
      auto* tmp = st.back().back();
      st.back().pop_back();
      return tmp;
   }

   menu_node* next_leaf_node()
   {
      while (std::empty(st.back()))
         if (!next_internal())
            return nullptr;

      return advance();
   }

   auto* next_node()
   {
      if (std::empty(st.back()))
         return next_internal();

      return advance();
   }

   auto get_depth() const noexcept
      { return static_cast<int>(std::size(st)) - 1; }

   template <int M = N>
   typename std::enable_if<M == 0, menu_node*>::type
   next()
      { return next_leaf_node(); }

   template <int M = N>
   typename std::enable_if<M == 1, menu_node*>::type
   next()
      { return next_node(); }
};

// Traverses the menu in the same order as it would appear in the
// config file.
class menu_traversal2 {
private:
   std::deque<std::deque<menu_node*>> st;
   int depth = -1;

public:
   menu_traversal2(menu_node* root, int depth_)
   : depth(depth_)
   {
      if (root)
         st.push_back({root});
   }

   auto* advance()
   {
      auto* node = st.back().back();
      st.back().pop_back();
      auto const ss = static_cast<int>(std::size(st));
      if (!std::empty(node->children) && ss <= depth)
         st.push_back(node->children);

      return node;
   }

   menu_node* next()
   {
      while (std::empty(st.back())) {
         st.pop_back();
         if (std::empty(st))
            return nullptr;
      }

      return advance();
   }

   auto get_depth() const noexcept
      { return static_cast<int>(std::size(st)) - 1; }

};

template <int N>
struct menu_iter_impl {
   using type = menu_traversal<N>;
};

template <>
struct menu_iter_impl<3> {
   using type = menu_traversal2;
};

template <int N>
class menu_iterator {
private:
   typename menu_iter_impl<N>::type iter;
   menu_node* current = nullptr;

public:
   using value_type = menu_node;
   using difference_type = std::ptrdiff_t;
   using reference = menu_node&;
   using const_reference = menu_node const&;
   using pointer = menu_node*;
   using const_pointer = menu_node const*;
   using iterator_category = std::forward_iterator_tag;

   menu_iterator( menu_node* root = nullptr
                , int depth = menu::max_supported_depth)
   : iter {root, depth}
   { 
      if (root)
         current = iter.advance();
   }

   reference operator*() { return *current;}
   const_reference const& operator*() const { return *current;}

   menu_iterator& operator++()
   {
      current = iter.next();
      return *this;
   }

   menu_iterator operator++(int)
   {
      menu_iterator ret(*this);
      ++(*this);
      return ret;
   }

   pointer operator->()
      {return current;}

   const_pointer operator->() const
      {return current;}

   friend
   auto operator==(menu_iterator const& a, menu_iterator const& b)
      { return a.current == b.current; }

   friend
   auto operator!=(menu_iterator const& a, menu_iterator const& b)
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
   using iterator = menu_iterator<N>;

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

inline
std::uint64_t to_channel_hash_code_d1(channel_code_type const& c)
{
   if (std::size(c) < 1)
      return 0;

   return c[0];
}

inline
std::uint64_t to_channel_hash_code_d2(channel_code_type const& c)
{
   if (std::size(c) < 2)
      return 0;

   std::uint64_t ca = c[0];
   std::uint64_t cb = c[1];

   constexpr auto shift = 32;

   ca <<= shift;
   return ca | cb;
}

inline
std::uint64_t to_channel_hash_code_d3(channel_code_type const& c)
{
   if (std::size(c) < 3)
      return 0;

   std::uint64_t ca = c[0];
   std::uint64_t cb = c[1];
   std::uint64_t cc = c[2];

   constexpr auto shift = 21;

   ca <<= 2 * shift;
   cb <<= 1 * shift;
   return ca | cb | cc;
}

inline
std::uint64_t to_channel_hash_code_d4(channel_code_type const& c)
{
   if (std::size(c) < 4)
      return 0;

   std::uint64_t ca = c[0];
   std::uint64_t cb = c[1];
   std::uint64_t cc = c[2];
   std::uint64_t cd = c[3];

   constexpr auto shift = 16;

   ca <<= 3 * shift;
   cb <<= 2 * shift;
   cc <<= 1 * shift;
   return ca | cb | cc | cd;
}

/* This function will hash a channel code in the form
 *
 *   __menu_1__    __menu_2__ 
 *  |          |  |          |
 * [[[1, 3,  4]], [[5, 1,  4]]]
 *
 * to an 64 bits integer which will be used for hashing. Each menu
 * configuration requires its own hashing function. This version is
 * suitable for two menus where both have filter depth 2 and has to be
 * extended for other sizes.
 */
inline
std::uint64_t to_hash_code(channel_code_type const& c, int depth)
{
   switch (depth) {
      case 1: return to_channel_hash_code_d1(c);
      case 2: return to_channel_hash_code_d2(c);
      case 3: return to_channel_hash_code_d3(c);
      case 4: return to_channel_hash_code_d4(c);
   }

   assert(false);
   return 0;
}

menu_channel_elem_type menu_elems_to_codes(menu_elem const& elem);

std::vector<int>
read_versions(menu_elems_array_type const& elems);

}


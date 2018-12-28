#include "menu.hpp"

#include <stack>
#include <limits>
#include <cctype>
#include <vector>
#include <iostream>
#include <algorithm>
#include <exception>
#include <ios>

#include "json_utils.hpp"

namespace rt
{

std::string to_str_raw(int i, int width, char fill)
{
   std::ostringstream oss;
   oss.fill(fill);
   oss.width(width);
   oss << std::hex << i;
   return oss.str();
}

std::string to_str(int i)
{
   return to_str_raw(i, 10, '0');
}

std::vector<std::string> get_hashes(std::string const& str)
{
   if (std::empty(str))
      return {};

   menu m {str};
   return m.get_codes_at_depth(2);
}

// TODO: Pass the field separator as argument to be able to read
// fields separated with ';' and not only spaces.
auto get_depth(std::string& line, menu::iformat f, char c)
{
   if (f == menu::iformat::spaces) {
      auto const i = line.find_first_not_of(" ");
      if (i == std::string::npos)
         throw std::runtime_error("Invalid line.");

      if (i % menu::sep != 0)
         throw std::runtime_error("Invalid indentation.");

      line.erase(0, i);
      return i / menu::sep;
   }

   if (f == menu::iformat::counter) {
      // WARNING: The parsing used here suports only up to a depth of
      // 10 (or 16 if we change to hex) where the digit indicating the
      // depth is only one character.

      auto const i = line.find_first_of(c);
      // To account for files terminating with newline.
      if (i == std::string::npos)
         return std::string::npos;

      auto const digit = line.substr(0, i);
      line.erase(0, i + 1);
      return std::stoul(digit);
   }

   return std::string::npos;
}

// Detects the file input format.
menu::iformat detect_iformat(std::string const& menu_str)
{
   int spaces = 0;
   int digits = 0;
   int none = 0;

   std::stringstream ss(menu_str);
   std::string line;
   while (std::getline(ss, line, '\n')) {
      if (std::empty(line))
         throw std::runtime_error("Invalid line.");

      if (line.front() == ' ')
         ++spaces;
      else if (std::isdigit(line.front()))
         ++digits;
      else
         ++none;
   }

   if (spaces == 0 && digits < 2 && none >= 1)
      return menu::iformat::spaces;

   if (spaces > 0) {
      assert(digits < 2);
      return menu::iformat::spaces;
   }

   assert(digits > 0);
   assert(spaces == 0);
   return menu::iformat::counter;
}

// Finds the max depth in a menu.
auto get_max_depth(std::string const& menu_str, menu::iformat f, char c)
{
   std::stringstream ss(menu_str);
   std::string line;
   unsigned max_depth = 0;
   while (std::getline(ss, line, '\n')) {
      auto const i = get_depth(line, f, c);

      if (i == std::string::npos)
         continue;

      if (max_depth < static_cast<unsigned>(i))
         max_depth = i;
   }

   return 1 + max_depth;
}

template <class Iter>
std::string get_code(Iter begin, Iter end)
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

auto build_menu_tree( menu_node& root, std::string const& menu_str
                    , menu::iformat f, char c)
{
   // TODO: Make it exception safe.

   auto const max_depth = get_max_depth(menu_str, f, c);
   if (max_depth == 0)
      return static_cast<unsigned>(0);

   std::stringstream ss(menu_str);
   std::string line;
   std::deque<int> codes(max_depth - 1, -1);
   std::stack<menu_node*> stack;
   unsigned last_depth = 0;
   bool root_found = false;
   while (std::getline(ss, line, '\n')) {
      if (std::empty(line))
         continue;

      auto const depth = get_depth(line, f, c);
      if (depth == std::string::npos)
         continue;

      if (!root_found) {
         root_found = true;
         root.name = line;
         auto* p = new menu_node {line, {}};
         root.children.push_front(p);
         stack.push(p);
         continue;
      }

      ++codes.at(depth - 1);
      for (unsigned i = depth; i < std::size(codes); ++i)
         codes[i] = -1;

      auto const code = get_code(std::begin(codes), std::begin(codes) + depth);

      if (depth > last_depth) {
         if (last_depth + 1 != depth)
            throw std::runtime_error("Forward Jump not allowed.");

         // We found the child of the last node pushed on the stack.
         auto* p = new menu_node {line, code};
         stack.top()->children.push_front(p);
         stack.push(p);
         ++last_depth;
      } else if (depth < last_depth) {
         // Now we have to pop that number of nodes from the stack
         // until we get to the node that is should be the parent of
         // the current line.
         auto const delta_depth = last_depth - depth;
         for (unsigned i = 0; i < delta_depth; ++i)
            stack.pop();

         stack.pop();

         // Now we can add the new node.
         auto* p = new menu_node {line, code};
         stack.top()->children.push_front(p);
         stack.push(p);

         last_depth = depth;
      } else {
         stack.pop();
         auto* p = new menu_node {line, code};
         stack.top()->children.push_front(p);
         stack.push(p);
         // Last depth stays equal.
      }
   }

   return max_depth;
}

// Iterator used to traverse the menu depth first.
class menu_iterator {
private:
   std::deque<std::deque<menu_node const*>> st;
   unsigned depth;

   void advance()
   {
      while (!std::empty(st.back().back()->children) &&
             std::size(st) <= depth) {
         std::deque<menu_node const*> tmp;
         for (auto o : st.back().back()->children)
            tmp.push_back(o);

         st.push_back(std::move(tmp));
      }

      current = st.back().back();
      st.back().pop_back();
   }

   void next_internal()
   {
      st.pop_back();
      if (std::empty(st))
         return;
      current = st.back().back();
      st.back().pop_back();
   }

public:
   menu_node const* current;
   menu_iterator(menu_node* root, unsigned depth_)
   : depth(depth_)
   , current {root}
   {
      if (root)
         st.push_back({root});

      advance();
   }

   auto get_depth() const noexcept { return std::size(st); }

   void next_leaf_node()
   {
      while (std::empty(st.back())) {
         next_internal();
         if (std::empty(st))
            return;
      }

      advance();
   }

   void next_node()
   {
      if (std::empty(st.back())) {
         next_internal();
         return;
      }

      advance();
   }

   bool end() const noexcept { return std::empty(st); }
};

menu::menu(std::string const& str)
{
   // TODO: Catch exceptions and release already acquired memory.
   // TODO: Automatically detect the line separator.

   auto const f = detect_iformat(str);
   char c = ' ';
   if (f == iformat::counter)
      c = ';';

   max_depth = build_menu_tree(root, str, f, c);
}

void node_dump( menu_node const& node, menu::oformat of
              , std::ostringstream& oss, int max_depth)
{
   auto const k =
      std::count(std::begin(node.code), std::end(node.code), '.');

   auto const indent = std::size(node.code) - k;
   if (of == menu::oformat::spaces) {
      std::string indent_str(indent, ' ');
      oss << indent_str << node.name;
      return;
   }

   if (of == menu::oformat::counter) {
      auto const k =  indent / menu::sep;
      oss << k << ';' << node.name;
      return;
   }

   if (of == menu::oformat::info) {
      auto const n = (max_depth - 1) * (menu::sep + 1);
      oss << std::setw(n) << std::left << node.code << "" << node.name;
      return;
   }

   oss << node.code;
}

std::string menu::dump(oformat of)
{
   // Traverses the menu in the same order as it would apear in the
   // config file.
   if (std::empty(root.children))
      return {};

   std::string output;
   std::deque<std::deque<menu_node*>> st;
   st.push_back(root.children);
   std::ostringstream oss;
   while (!std::empty(st)) {
      auto* node = st.back().back();
      node_dump(*node, of, oss, max_depth);
      oss << '\n';
      st.back().pop_back();
      if (std::empty(st.back()))
         st.pop_back();
      if (std::empty(node->children))
         continue;
      st.push_back(node->children);
   }

   return oss.str();
}

std::vector<std::string> menu::get_codes_at_depth(unsigned depth) const
{
   if (std::empty(root.children))
      return {};

   std::vector<std::string> ret;
   menu_iterator iter(root.children.front(), depth);
   while (!iter.end()) {
      ret.push_back(iter.current->code);
      iter.next_leaf_node();
   }

   return ret;
}

bool menu::check_leaf_min_depths(unsigned min_depth) const
{
   // TODO: Change the function to return an iterator to the
   // invalid node.

   if (std::empty(root.children))
      return {};

   auto const max = std::numeric_limits<unsigned>::max();
   menu_iterator iter(root.children.front(), max);
   while (!iter.end()) {
      auto const d = iter.get_depth();
      if (d < min_depth + 1)
         return false;
      iter.next_leaf_node();
   }

   return true;
}

menu::~menu()
{
   if (!std::empty(root.children)) {
      auto const max = std::numeric_limits<unsigned>::max();
      menu_iterator iter(root.children.front(), max);
      while (!iter.end()) {
         delete iter.current;
         iter.next_node();
      }
   }
}

}


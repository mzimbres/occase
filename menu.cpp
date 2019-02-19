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

// TODO: Pass the field separator as argument to be able to read
// fields separated with ';' and not only spaces.
auto remove_depth(std::string& line, menu::iformat ifmt)
{
   if (ifmt == menu::iformat::spaces) {
      auto const i = line.find_first_not_of(' ');
      if (i == std::string::npos)
         throw std::runtime_error("Invalid line.");

      if (i % menu::sep != 0)
         throw std::runtime_error("Invalid indentation.");

      line.erase(0, i);
      return static_cast<int>(i) / menu::sep;
   }

   if (ifmt == menu::iformat::counter) {
      // WARNING: The parser used here suports only up to a depth of
      // 10 (or 16 if we change to hex) where the digit indicating the
      // depth is only one character.

      // We have to skip empty lines.
      if (std::empty(line))
         return -1;

      // This input format requires the presence of ';'
      auto const p1 = line.find_first_of(';');
      if (p1 == std::string::npos)
         throw std::runtime_error("Invalid line.");

      auto const p2 = line.find_first_of(';', p1 + 1);
      if (p2 == std::string::npos)
         throw std::runtime_error("Invalid line.");

      // The middle data cannot be empty.
      if (p2 == p1 + 1)
         throw std::runtime_error("Invalid line.");

      auto const digit = line.substr(0, p1);
      line.erase(0, p1 + 1);
      line.erase(p2);

      // Now the line contains only the middle field.
      return std::stoi(digit);
   }

   return -1;
}

// Detects the file input format.
menu::iformat detect_iformat(std::string const& menu_str)
{
   auto const n = std::count( std::begin(menu_str), std::end(menu_str)
                            , '=');
   if (n > 1)
      return menu::iformat::counter;

   // TODO: Implement support for counter format with newline
   // separator.

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

/*
 * Returns the menu depth. The root node is excluded that means, a
 * menu in the form
 *
 * foo
 *    bar
 *    bar
 *       foobar
 *       bar
 *
 * A menu in the form
 *
 * bar
 *
 * will have depth 0. An empty menu will have depth -1.  has depth 2.
 * This is the number of fields in the code that is needed to identify
 * a node in the tree.
 */
auto get_max_depth( std::string const& menu_str
                  , menu::iformat ifmt)
{
   char const line_sep = ifmt == menu::iformat::spaces ? '\n' : '=';
   std::stringstream ss(menu_str);
   std::string line;
   int max_depth = -1;
   while (std::getline(ss, line, line_sep)) {
      auto const i = remove_depth(line, ifmt);

      if (i == -1)
         continue;

      if (max_depth < i)
         max_depth = i;
   }

   return max_depth;
}

// Parses the three contained in menu_str and puts its root node in
// root.children.
auto parse_tree( menu_node& head
               , std::string const& menu_str
               , menu::iformat ifmt)
{
   // TODO: Make it exception safe.

   auto const max_depth = get_max_depth(menu_str, ifmt);
   if (max_depth == -1) {
      // The menu is empty.
      return -1;
   }

   std::stringstream ss(menu_str);
   std::string line;
   std::vector<int> codes(max_depth, -1);
   std::stack<menu_node*> stack;
   int last_depth = 0;
   char const line_sep = ifmt == menu::iformat::spaces ? '\n' : '=';
   while (std::getline(ss, line, line_sep)) {
      if (std::empty(line))
         continue;

      auto const depth = remove_depth(line, ifmt);
      if (depth == -1)
         continue;

      if (std::empty(head.children)) {
         auto* p = new menu_node {line, {}};
         head.children.push_front(p);
         stack.push(p);
         continue;
      }

      ++codes.at(depth - 1);
      for (unsigned i = depth; i < std::size(codes); ++i)
         codes[i] = -1;

      std::vector<int> const code { std::begin(codes)
                                  , std::begin(codes) + depth};
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
         for (auto i = 0; i < delta_depth; ++i)
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

menu::menu(std::string const& str)
{
   // TODO: Catch exceptions and release already acquired memory.
   // TODO: Automatically detect the line separator.

   auto const ifmt = detect_iformat(str);
   max_depth = parse_tree(head, str, ifmt);

   auto acc = [](auto a, auto const* p)
   {
      if (std::empty(p->children))
         return a + 1;
      return a + p->leaf_counter;
   };

   for (auto& o : menu_view<1> {head.children.front()}) {
      if (std::empty(o.children))
         o.leaf_counter = 0;
      else
         o.leaf_counter =
            std::accumulate( std::begin(o.children)
                           , std::end(o.children)
                           , 0, acc);
   }
}

void node_dump( menu_node const& node, menu::oformat of
              , std::ostringstream& oss, int max_depth)
{
   if (of == menu::oformat::spaces) {
      std::string indent_str(std::size(node.code) * menu::sep, ' ');
      oss << indent_str << node.name;
      return;
   }

   if (of == menu::oformat::counter) {
      oss << std::size(node.code) << ';'
          << node.name << ';' << node.leaf_counter;
      return;
   }

   if (of == menu::oformat::info) {
      auto const n = max_depth * (menu::sep + 1);
      oss << std::setw(n) << std::left
          << get_code_as_str(node.code) << ' '
          << node.name << ' ' << node.leaf_counter;
      return;
   }

   oss << get_code_as_str(node.code);
}

std::string
menu::dump(oformat of, char line_sep, int const max_depth)
{
   std::string output;
   std::ostringstream oss;

   menu_traversal2 mt {head, max_depth};

   while (!mt.end()) {
      node_dump(*mt.current, of, oss, max_depth);
      oss << line_sep;
      mt.next();
   }

   return oss.str();
}

bool check_leaf_min_depths(menu& m, int min_depth)
{
   // TODO: Change the function to return an iterator to the
   // invalid node.

   menu_view<0> view {m, min_depth};
   for (auto iter = std::begin(view); iter != std::end(view); ++iter) {
      auto const d = iter.get_depth();
      if (d < min_depth)
         return false;
   }

   return true;
}

menu::~menu()
{
   menu_view<1> view {head.children.front()};
   for (auto iter = std::begin(view); iter != std::end(view); ++iter)
      delete iter.get_pointer_to_node();
}

void to_json(json& j, menu_elem const& e)
{
  j = json{{"data", e.data}, {"depth", e.depth}, {"version", e.version}};
}

void from_json(json const& j, menu_elem& e)
{
  e.data = j.at("data").get<std::string>();
  e.depth = j.at("depth").get<unsigned>();
  e.version = j.at("version").get<int>();
}

// Stolen from rtcpp.
template <class Iter>
auto next_tuple( Iter begin, Iter end
               , Iter min, Iter max)
{
    auto j = end - begin - 1;
    while (begin[j] == max[j]) {
      begin[j] = min[j];
      --j;
    }
    ++begin[j];
    
    return j != 0;
}

std::vector<std::vector<std::vector<std::vector<int>>>>
channel_codes( std::vector<std::vector<std::vector<int>>> const& channels
             , std::vector<menu_elem> const& menus)
{
   if (std::empty(channels))
      return {};

   for (auto const& o : channels)
      if (std::empty(o))
         return {};

   // TODO: Make min and max const.
   std::vector<unsigned> min(1 + std::size(channels), 0);
   std::vector<unsigned> max(1, min[0] + 1);
   for (auto const& o : channels)
      max.push_back(std::size(o) - 1);
   
   auto comb = min;
   std::vector<std::vector<std::vector<std::vector<int>>>> ret;
   do {
      std::vector<std::vector<std::vector<int>>> foo;
      for (unsigned i = 0; i < std::size(channels); ++i) {
         foo.push_back(
               { std::vector<int>(
                     std::begin(channels.at(i).at(comb.at(1 + i))),
                     std::begin(channels.at(i).at(comb.at(1 + i)))
                         + menus.at(i).depth)
               });
      }

      ret.push_back(std::move(foo));
   } while (next_tuple( std::begin(comb), std::end(comb)
                      , std::begin(min), std::begin(max)));
   return ret;
}

std::uint64_t convert_to_channel_code(
      std::vector<std::vector<std::vector<int>>> const& codes)
{
   if (std::empty(codes))
      return {};

   assert(std::size(codes.front()) == 1);
   assert(std::size(codes) == 2);

   assert(std::size(codes.front().front()) == 2);
   assert(std::size(codes.front().back()) == 2);
   
   // First menu.
   std::uint64_t c1a = codes.front().front().front();
   std::uint64_t c1b = codes.front().front().back();

   // Second menu.
   std::uint64_t c2a = codes.back().front().front();
   std::uint64_t c2b = codes.back().front().back();

   c1a <<= 48;
   c1b <<= 32;
   c2a <<= 16;
   // c2b is already in the correct position.

   return c1a | c1b | c2a | c2b;
}

std::vector<std::vector<std::vector<int>>>
menu_elems_to_codes(std::vector<menu_elem> const& elems)
{
   // First we collect the codes from each menu at the desired depth.
   std::vector<std::vector<std::vector<int>>> hash_codes;
   for (auto const& elem : elems) {
      menu m {elem.data};
      if (std::empty(m))
         throw std::runtime_error("Menu is empty.");

      std::vector<std::vector<int>> codes;
      for (auto const& o : menu_view<0> {m, elem.depth})
         codes.push_back(o.code);

      if (std::empty(codes))
         throw std::runtime_error("Invalid menu.");

      hash_codes.push_back(std::move(codes));
   }

   if (std::empty(hash_codes))
      throw std::runtime_error("Menus is empty.");

   return hash_codes;
}

std::vector<int>
read_versions(std::vector<menu_elem> const& elems)
{
   std::vector<int> vs;
   for (auto const& e : elems)
      vs.push_back(e.version);

   return vs;
}

}


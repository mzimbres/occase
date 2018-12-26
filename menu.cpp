#include "menu.hpp"

#include <stack>
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

struct patch_helper {
   json j;
   std::string path_prefix;
   std::string value_prefix;
   std::vector<std::string> header;
};

struct hash_gen_iter {
   std::stack<std::vector<patch_helper>> st;
   patch_helper current;

   hash_gen_iter(json j)
   {
      st.push({{j, "", "00", {}}});
      advance();
   }

   void next()
   {
      while (std::empty(st.top())) {
         st.pop();
         if (std::empty(st))
            return;
         st.top().pop_back();
      }

      advance();
   }

   void advance()
   {
      while (!st.top().back().j["sub"].is_null()) {
         auto const sub_desc =
            st.top().back().j["sub_desc"].get<std::string>();
         std::vector<patch_helper> tmp;
         auto i = 0;
         for (auto o : st.top().back().j["sub"]) {
            auto const path = st.top().back().path_prefix
                            + "/sub/"
                            + std::to_string(i);
            auto const value = st.top().back().value_prefix
                             + "."
                             + to_str_raw(i, 2, '0');
            auto header = st.top().back().header;
            auto const category = sub_desc + " : "
                                + o["name"].get<std::string>();
            header.push_back(category);
            tmp.push_back({o, path, value, header});
            ++i;
         }

         st.push(std::move(tmp));
      }

      current = st.top().back();
      st.top().pop_back();
   }

   bool end() const noexcept
   {
      return std::empty(st);
   }
};

std::vector<std::string> get_hashes(json menu)
{
   if (std::empty(menu))
      return {};

   std::vector<std::string> hashes;
   hash_gen_iter iter(menu);
   while (!iter.end()) {
      hashes.push_back(iter.current.value_prefix);
      iter.next();
   };

   return hashes;
}

std::vector<json> gen_hash_patches(json menu)
{
   if (std::empty(menu))
      return {};

   std::vector<json> patches;
   hash_gen_iter iter(menu);
   while (!iter.end()) {
      json patch;
      patch["op"] = "replace";
      patch["path"] = iter.current.path_prefix + "/hash";
      patch["value"] = iter.current.value_prefix;
      patches.push_back(patch);
      iter.next();
   };

   return patches;
}

std::string gen_sim_menu(int l)
{
   std::string const sep = "   ";
   std::string str;
   str += "Root\n";
   for (auto i = 0; i < l; ++i) {
      str += sep + "foo\n";
      for (auto j = 0; j < l; ++j) {
         str += sep + sep + "bar\n";
         for (auto j = 0; j < l; ++j)
            str += sep + sep + sep + "foobar\n";
      }
   }

   return str;
}

auto get_depth(std::string& line, int sep)
{
   auto const i = line.find_first_not_of(" ");
   if (i == std::string::npos)
      throw std::runtime_error("Invalid line.");

   if (i % sep != 0)
      throw std::runtime_error("Invalid indentation.");

   line.erase(0, i);
   return i / sep;
}

// Finds the max depth in a menu.
auto get_max_depth(std::string const& menu_str, int sep)
{
   std::stringstream ss(menu_str);
   std::string line;
   auto max_depth = 0;
   while (std::getline(ss, line)) {
      auto const i = line.find_first_not_of(" ");
      if (i == std::string::npos)
         throw std::runtime_error("Invalid line.");
      if (max_depth < static_cast<int>(i))
         max_depth = i;
   }

   if (max_depth % sep != 0)
      throw std::runtime_error("Invalid indentation.");

   return 1 + max_depth / sep;
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

void build_menu_tree(menu_node& root, std::string const& menu_str)
{
   // TODO: Make it exception safe.

   auto const max_depth = get_max_depth(menu_str, 3);
   //std::cout << "Max depth: " << max_depth << std::endl;
   if (max_depth == 0)
      return;

   std::stringstream ss(menu_str);
   std::string line;
   std::deque<int> codes(max_depth - 1, -1);
   std::stack<menu_node*> stack;
   unsigned last_depth = 0;
   bool root_found = false;
   while (std::getline(ss, line)) {
      if (line.front() != ' ') {
         // This is the root node since it is the only one with zero
         // indentation.
         if (root_found)
            throw std::runtime_error("Invalid input data.");

         // From now on no other node will be allowed to have zero
         // indentation.
         root_found = true;
         root.name = line;
         auto* p = new menu_node {line, {}};
         root.children.push_front(p);
         stack.push(p);
         continue;
      }

      auto const depth = get_depth(line, 3);
      ++codes.at(depth - 1);
      for (unsigned i = depth; i < std::size(codes); ++i)
         codes[i] = -1;

      auto const c = get_code(std::begin(codes), std::begin(codes) + depth);

      if (depth > last_depth) {
         if (last_depth + 1 != depth)
            throw std::runtime_error("Forward Jump not allowed.");

         // We found the child of the last node pushed on the stack.
         auto* p = new menu_node {line, c};
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
         auto* p = new menu_node {line, c};
         stack.top()->children.push_front(p);
         stack.push(p);

         last_depth = depth;
      } else {
         stack.pop();
         auto* p = new menu_node {line, c};
         stack.top()->children.push_front(p);
         stack.push(p);
         // Last depth stays equal.
      }
   }
}

// Iterator used to traverse the menu depth first.
class menu_iterator {
private:
   // Since it is not possible to iterate over a stack I will use a
   // deque.
   std::deque<std::deque<menu_node*>> st;

   void advance()
   {
      while (!std::empty(st.back().back()->children)) {
         std::deque<menu_node*> tmp;
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
   menu_node* current;
   menu_iterator(menu_node* root)
   : current {root}
   {
      if (root)
         st.push_back({root});

      advance();
   }

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

menu::menu(std::string const& str, format)
{
   // TODO: Catch exceptions and release already acquired memory.
   build_menu_tree(root, str);
}

void menu::print_leaf()
{
   menu_iterator iter(root.children.front());
   while (!iter.end()) {
      std::cout << std::setw(20) << std::left
                << iter.current->name << " "
                << iter.current->code << "      "
                << std::endl;
      iter.next_leaf_node();
   }
}

std::vector<std::string> menu::get_leaf_codes() const
{
   std::vector<std::string> ret;
   menu_iterator iter(root.children.front());
   while (!iter.end()) {
      ret.push_back(iter.current->code);
      iter.next_leaf_node();
   }

   return ret;
}

std::string
node_dump(menu_node const& node, int type, bool hash)
{
   auto const n =
      std::count( std::begin(node.code)
                , std::end(node.code)
                , '.');

   auto const indent = std::size(node.code) - n;
   if (type == 1) {
      std::ostringstream oss;

      std::string indent_str(indent, ' ');
      oss << indent_str << node.name;

      if (hash)
         oss << " " << node.code;

      return oss.str();
   }

   if (type == 2) {
      std::ostringstream oss;

      auto const k =  indent / menu::sep;
      oss << k << " " << node.name;
      if (hash)
         oss << " " << node.code;

      return oss.str();
   }

   //if (type == 3)
      return node.code;
}

std::string menu::dump(int type, bool hash)
{
   // Traverses the menu in the same order as it would apear in the
   // config file.
   std::string output;
   std::deque<std::deque<menu_node*>> st;
   st.push_back(root.children);
   while (!std::empty(st)) {
      auto* node = st.back().back();
      output += node_dump(*node, type, hash);
      output += "\n";
      st.back().pop_back();
      if (std::empty(st.back()))
         st.pop_back();
      if (std::empty(node->children))
         continue;
      st.push_back(node->children);
   }

   return output;
}

menu::~menu()
{
   menu_iterator iter(root.children.front());
   while (!iter.end()) {
      delete iter.current;
      iter.next_node();
   }
}

}


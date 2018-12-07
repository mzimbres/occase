#include "menu.hpp"

#include <stack>
#include <vector>
#include <iostream>
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
      if (!std::empty(st.top())) {
         advance();
         return;
      }

      do {
         st.pop();
         if (std::empty(st))
            return;
         st.top().pop_back();
      } while (std::empty(st.top()));

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

std::vector<json> gen_sim_leaf_node(int size, std::string name_prefix)
{
   std::vector<json> jv;
   for (auto i = 0; i < size; ++i) {
      auto const name = name_prefix + std::to_string(i);
      jv.push_back({{"hash", ""}, {"sub", {}}, {"name", name}});
   }

   return jv;
}

json gen_sim_menu(int l)
{
   json j;
   j["name"] = "Root";
   j["sub_desc"] = "Root children";
   j["menu_version"] = 1;

   std::vector<json> js;
   for (auto i = 0; i < l; ++i) {
      auto const name1 = std::to_string(i);
      std::vector<json> js1;
      for (auto j = 0; j < l; ++j) {
         auto const name2 = name1 + "." + std::to_string(j);
         json j_tmp2;
         j_tmp2["name"] = name2;
         j_tmp2["sub_desc"] = "Children";
         j_tmp2["sub"] = gen_sim_leaf_node(l, name2 + ".");
         js1.push_back(j_tmp2);
      }
      json j_tmp;
      j_tmp["name"] = name1;
      j_tmp["sub_desc"] = "Children";
      j_tmp["sub"] = js1;
      js.push_back(j_tmp);
   }

   j["sub"] = js;

   auto const hash_patches = gen_hash_patches(j);
   return j.patch(std::move(hash_patches));
}

json gen_location_menu()
{
   std::vector<json> j1 =
   { {{"hash", ""}, {"sub", {}}, {"name", "Centro"}               }
   , {{"hash", ""}, {"sub", {}}, {"name", "Alvinópolis"}          }
   , {{"hash", ""}, {"sub", {}}, {"name", "Jardim Siriema"}       }
   , {{"hash", ""}, {"sub", {}}, {"name", "Vila Santista"}        }
   , {{"hash", ""}, {"sub", {}}, {"name", "Parque dos Coqueiros"} }
   , {{"hash", ""}, {"sub", {}}, {"name", "Terceiro Centenário"}  }
   };

   std::vector<json> j2 =
   { {{"hash", ""}, {"sub", {}}, {"name", "Vila Leopoldina"}, }
   , {{"hash", ""}, {"sub", {}}, {"name", "Lapa"},            }
   , {{"hash", ""}, {"sub", {}}, {"name", "Pinheiros"},       }
   , {{"hash", ""}, {"sub", {}}, {"name", "Moema"},           }
   , {{"hash", ""}, {"sub", {}}, {"name", "Jardim Paulista"}, }
   , {{"hash", ""}, {"sub", {}}, {"name", "Mooca"},           }
   , {{"hash", ""}, {"sub", {}}, {"name", "Tatuapé"},         }
   , {{"hash", ""}, {"sub", {}}, {"name", "Penha"},           }
   , {{"hash", ""}, {"sub", {}}, {"name", "Ipiranga"},        }
   , {{"hash", ""}, {"sub", {}}, {"name", "Vila Madalena"},   }
   , {{"hash", ""}, {"sub", {}}, {"name", "Vila Mariana"},    }
   , {{"hash", ""}, {"sub", {}}, {"name", "Vila Formosa"},    }
   , {{"hash", ""}, {"sub", {}}, {"name", "Bixiga"},          }
   };

   std::vector<json> j3 =
   { {{"name", "Atibaia"},  {"sub_desc", "Bairro"}, {"sub", j1}}
   , {{"name", "Sao Paulo"},{"sub_desc", "Bairro"}, {"sub", j2}}
   };

   json j;
   j["name"] = "SP";
   j["sub_desc"] = "Cidade";
   j["menu_version"] = 1;
   j["sub"] = j3;

   auto const hash_patches = gen_hash_patches(j);
   return j.patch(std::move(hash_patches));
}

struct helper {
   menu_node* node = nullptr;;
   int counter = 0;
};

void build_menu_tree(menu_node& root, std::string const& menu_str)
{
   // TODO: Make it exception safe.
   constexpr auto sep = 3;
   std::stringstream ss(menu_str);
   std::string line;
   std::stack<helper> stack;
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
         stack.push({&root, 0});
         continue;
      }

      auto const pos = line.find_first_not_of(" ");
      if (pos == std::string::npos)
         throw std::runtime_error("Invalid line.");

      if (pos % sep != 0)
         throw std::runtime_error("Invalid indentation.");

      line.erase(0, pos);
      auto const dist = pos;
      auto const last_dist = sep * last_depth;
      if (dist > last_dist) {
         if (last_dist + sep != dist)
            throw std::runtime_error("Forward Jump not allowed.");

         // We found the child of the last node pushed on the stack.
         auto const code = stack.top().node->code + ".000";
         auto* p = new menu_node {line, code, {}};
         stack.top().node->children.push_front(p);
         stack.push({p, 0});
         ++last_depth;
      } else if (dist < last_dist) {
         // We do not know how may indentations back we jumped. Let us
         // calculate this.
         auto const new_depth = dist / sep;
         // Now we have to pop that number of nodes from the stack
         // until we get to the node that is should be the parent of
         // the current line.
         auto const delta_depth = last_depth - new_depth;
         for (unsigned i = 0; i < delta_depth; ++i)
            stack.pop();

         auto const new_counter = stack.top().counter + 1;
         stack.pop();

         // Now we can add the new node.
         auto const code = stack.top().node->code 
                         + "."
                         + to_str_raw(new_counter, 3, '0');
         auto* p = new menu_node {line, code, {}};
         stack.top().node->children.push_front(p);
         stack.push({p, new_counter});

         last_depth = new_depth;
      } else {
         auto const new_counter = stack.top().counter + 1;
         stack.pop();
         auto const code = stack.top().node->code 
                         + "."
                         + to_str_raw(new_counter, 3, '0');
         auto* p = new menu_node {line, code, {}};
         stack.top().node->children.push_front(p);
         stack.push({p, new_counter});
         // Last depth stays equal.
      }
   }
}

menu_leaf_iterator::menu_leaf_iterator(menu_node& root)
: current {&root}
{
   st.push({&root});
   advance();
}

void menu_leaf_iterator::next_leaf()
{
   if (std::empty(st.top())) {
      do {
         st.pop();
         if (std::empty(st))
            return;
         st.top().pop_back();
      } while (std::empty(st.top()));
   }

   advance();
}

void menu_leaf_iterator::advance()
{
   while (!std::empty(st.top().back()->children)) {
      std::vector<menu_node*> tmp;
      for (auto o : st.top().back()->children)
         tmp.push_back(o);

      st.push(std::move(tmp));
   }

   current = st.top().back();
   st.top().pop_back();
}

}


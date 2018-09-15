#include "menu_parser.hpp"

#include <stack>
#include <vector>
#include <iostream>

json gen_location_menu1()
{
   std::vector<json> j3 =
   { {{"status", "off"}, {"hash", ""}, {"name", "Atibaia"},   {"sub", {}}}
   , {{"status", "off"}, {"hash", ""}, {"name", "Sao Paulo"}, {"sub", {}}}
   };

   json j;
   j["name"] = "SP";
   j["sub_desc"] = "Cidades";
   j["sub"] = j3;

   return j;
}

json gen_location_menu()
{
   std::vector<json> j1 =
   { {{"status", "off"}, {"hash", ""}, {"sub", {}}, {"name", "Centro"}               }
   , {{"status", "off"}, {"hash", ""}, {"sub", {}}, {"name", "Alvinópolis"}          }
   , {{"status", "off"}, {"hash", ""}, {"sub", {}}, {"name", "Jardim Siriema"}       }
   , {{"status", "off"}, {"hash", ""}, {"sub", {}}, {"name", "Vila Santista"}        }
   , {{"status", "off"}, {"hash", ""}, {"sub", {}}, {"name", "Parque dos Coqueiros"} }
   , {{"status", "off"}, {"hash", ""}, {"sub", {}}, {"name", "Terceiro Centenário"}  }
   };

   std::vector<json> j2 =
   { {{"status", "off"}, {"hash", ""}, {"sub", {}}, {"name", "Vila Leopoldina"}, }
   , {{"status", "off"}, {"hash", ""}, {"sub", {}}, {"name", "Lapa"},            }
   , {{"status", "off"}, {"hash", ""}, {"sub", {}}, {"name", "Pinheiros"},       }
   , {{"status", "off"}, {"hash", ""}, {"sub", {}}, {"name", "Moema"},           }
   , {{"status", "off"}, {"hash", ""}, {"sub", {}}, {"name", "Jardim Paulista"}, }
   , {{"status", "off"}, {"hash", ""}, {"sub", {}}, {"name", "Mooca"},           }
   , {{"status", "off"}, {"hash", ""}, {"sub", {}}, {"name", "Tatuapé"},         }
   , {{"status", "off"}, {"hash", ""}, {"sub", {}}, {"name", "Penha"},           }
   , {{"status", "off"}, {"hash", ""}, {"sub", {}}, {"name", "Ipiranga"},        }
   , {{"status", "off"}, {"hash", ""}, {"sub", {}}, {"name", "Vila Madalena"},   }
   , {{"status", "off"}, {"hash", ""}, {"sub", {}}, {"name", "Vila Mariana"},    }
   , {{"status", "off"}, {"hash", ""}, {"sub", {}}, {"name", "Vila Formosa"},    }
   , {{"status", "off"}, {"hash", ""}, {"sub", {}}, {"name", "Bixiga"},          }
   };

   std::vector<json> j3 =
   { {{"name", "Atibaia"},  {"sub_desc", "Bairros"}, {"sub", j1}}
   , {{"name", "Sao Paulo"},{"sub_desc", "Bairros"}, {"sub", j2}}
   };

   json j;
   j["name"] = "SP";
   j["sub_desc"] = "Cidades";
   j["sub"] = j3;

   return j;
}

std::string to_str(int i, int width, char fill_char)
{
   std::ostringstream oss;
   oss.fill(fill_char);
   oss.width(width);
   oss << i;
   return oss.str();
}

struct menu_leaf_iter {
   std::stack<std::vector<std::pair<std::string, json>>> st;
   std::pair<std::string, json> current;

   menu_leaf_iter(json j, std::string prefix)
   {
      st.push({{prefix, j}});
      next();
   }

   void next()
   {
      while (!st.top().back().second["sub"].is_null()) {
         auto const sub = st.top().back().second["sub"];
         std::vector<std::pair<std::string, json>> tmp;
         int i = 0;
         for (auto o : sub) {
            auto str = st.top().back().first;
            str.append(".");
            str.append(to_str(i++, 2, '0'));
            tmp.push_back({std::move(str), o});
         }
         st.push(std::move(tmp));
      }

      current = st.top().back();

      st.top().pop_back();

      if (!std::empty(st.top()))
         return;
      
      st.pop();
      st.top().pop_back();

      if (std::empty(st.top()))
         st.pop();
   }

   bool end() const noexcept {return std::size(st) == 1;}
};

std::stack<std::string>
parse_menu_json(json j, user_bind bind, std::string prefix)
{
   if (std::empty(j))
      return {};

   menu_leaf_iter iter(j, prefix);
   std::stack<std::string> hashes;
   while (!iter.end()) {
      iter.next();
      json jcmd;
      jcmd["cmd"] = "create_group";
      jcmd["from"] = bind;
      jcmd["hash"] = iter.current.first;

      hashes.push(jcmd.dump());
   };

   return hashes;
}

struct patch_helper {
   json j;
   std::string path_prefix;
   std::string value_prefix;
};

struct hash_gen_iter {
   std::stack<std::vector<patch_helper>> st;
   patch_helper current;

   hash_gen_iter(json j)
   {
      st.push({{j, "", "00"}});
      advance();
   }

   void next()
   {
      if (!std::empty(st.top())) {
         advance();
         return;
      }

      for (;;) {
         st.pop();
         if (std::empty(st))
            return;
         st.top().pop_back();
         if (!std::empty(st.top()))
            break;
      }
   }

   void advance()
   {
      while (!st.top().back().j["sub"].is_null()) {
         std::vector<patch_helper> tmp;
         auto i = 0;
         for (auto o : st.top().back().j["sub"]) {
            auto const path = st.top().back().path_prefix
                            + "/sub/"
                            + std::to_string(i);
            auto const value = st.top().back().value_prefix
                             + "."
                             + to_str(i, 2, '0');
            tmp.push_back({o, path, value});
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

std::vector<json> json_patches(json j)
{
   if (std::empty(j))
      return {};

   std::vector<json> patches;
   hash_gen_iter iter(j);
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


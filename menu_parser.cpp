#include "menu_parser.hpp"

#include <stack>
#include <vector>
#include <iostream>

std::string gen_menu_json(int indentation)
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

   return j.dump(indentation);
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
parse_menu_json(std::string menu, user_bind bind, std::string prefix)
{
   //std::cout << menu << std::endl;

   if (std::empty(menu))
      return {};

   json j = json::parse(menu);

   menu_leaf_iter iter(j, prefix);
   std::stack<std::string> hashes;
   while (!iter.end()) {
      iter.next();
      json jcmd;
      jcmd["cmd"] = "create_group";
      jcmd["from"] = bind;
      jcmd["hash"] = iter.current.first;
      jcmd["info"] = group_info {"Atibaia", "Centro"};

      hashes.push(jcmd.dump());
   };

   return hashes;
}

struct patch_helper {
   json j;
   std::string path_prefix;
   std::string value_prefix;
};

struct hashfy_iter {
   std::stack<std::vector<patch_helper>> st;
   patch_helper current;

   hashfy_iter(json j)
   {
      st.push({{j, "", "00"}});
      next();
   }

   void next()
   {
      while (!st.top().back().j["sub"].is_null()) {
         auto const sub = st.top().back().j["sub"];
         std::vector<patch_helper> tmp;
         auto i = 0;
         auto const path_prefix = st.top().back().path_prefix + "/sub/";
         auto const value_prefix = st.top().back().value_prefix;
         for (auto o : sub) {
            auto path_copy = path_prefix;
            auto value_copy = value_prefix;
            path_copy += std::to_string(i);
            value_copy += ".";
            value_copy += to_str(i, 2, '0');
            tmp.push_back({o, path_copy, value_copy});
            ++i;
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

void json_patches(std::string menu)
{
   if (std::empty(menu))
      return;

   json j = json::parse(menu);

   hashfy_iter iter(j);
   while (!iter.end()) {
      iter.next();
      json patch;
      patch["op"] = "replace";
      patch["path"] = iter.current.path_prefix + "/hash";
      patch["value"] = iter.current.value_prefix;
      std::cout << patch.dump() << std::endl;
   };
}


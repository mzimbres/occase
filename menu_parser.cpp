#include "menu_parser.hpp"

#include <stack>
#include <vector>
#include <iostream>

#include "json_utils.hpp"

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
   { {{"name", "Atibaia"},  {"sub_desc", "Bairro"}, {"sub", j1}}
   , {{"name", "Sao Paulo"},{"sub_desc", "Bairro"}, {"sub", j2}}
   };

   json j;
   j["name"] = "SP";
   j["sub_desc"] = "Cidade";
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
                             + to_str(i, 2, '0');
            auto header = st.top().back().header;
            auto const category = sub_desc + " : " + o["name"].get<std::string>();
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

std::vector<json> gen_hash_patches(json j)
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

std::vector<std::string> gen_create_groups(json menu, user_bind bind)
{
   if (std::empty(menu))
      return {};

   std::vector<std::string> cmds;
   hash_gen_iter iter(menu);
   while (!iter.end()) {
      json cmd;
      cmd["cmd"] = "create_group";
      cmd["from"] = bind;
      cmd["hash"] = iter.current.value_prefix;
      cmds.push_back(cmd.dump());
      iter.next();
   };

   return cmds;
}

json gen_group_info(json menu)
{
   if (std::empty(menu))
      return {};

   std::vector<group_info> infos;
   hash_gen_iter iter(menu);
   while (!iter.end()) {
      infos.push_back({iter.current.header, iter.current.value_prefix});
      iter.next();
   };

   json j_infos;
   j_infos["cmd"] = "group_infos";
   j_infos["infos"] = infos;
   return j_infos;
}


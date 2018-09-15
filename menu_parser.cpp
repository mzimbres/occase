#include "menu_parser.hpp"

#include <stack>
#include <vector>
#include <iostream>

std::string gen_menu_json(int indentation)
{
   std::vector<json> j1 =
   { {{"name", "Centro"},               {"sub", {}}}
   , {{"name", "Alvinópolis"},          {"sub", {}}}
   , {{"name", "Jardim Siriema"},       {"sub", {}}}
   , {{"name", "Vila Santista"},        {"sub", {}}}
   , {{"name", "Parque dos Coqueiros"}, {"sub", {}}}
   , {{"name", "Terceiro Centenário"},  {"sub", {}}}
   };

   std::vector<json> j2 =
   { {{"name", "Vila Leopoldina"}, {"sub", {}}}
   , {{"name", "Lapa"},            {"sub", {}}}
   , {{"name", "Pinheiros"},       {"sub", {}}}
   , {{"name", "Moema"},           {"sub", {}}}
   , {{"name", "Jardim Paulista"}, {"sub", {}}}
   , {{"name", "Mooca"},           {"sub", {}}}
   , {{"name", "Tatuapé"},         {"sub", {}}}
   , {{"name", "Penha"},           {"sub", {}}}
   , {{"name", "Ipiranga"},        {"sub", {}}}
   , {{"name", "Vila Madalena"},   {"sub", {}}}
   , {{"name", "Vila Mariana"},    {"sub", {}}}
   , {{"name", "Vila Formosa"},    {"sub", {}}}
   , {{"name", "Bixiga"},          {"sub", {}}}
   };

   std::vector<json> j3 =
   { {{"name", "Atibaia"},  {"sub", j1}}
   , {{"name", "Campinas"}, {"sub", {}}}
   , {{"name", "Sao Paulo"},{"sub", j2}}
   , {{"name", "Piracaia"}, {"sub", {}}}
   };

   json j;
   j["name"] = "SP";
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

std::stack<std::string>
parse_menu_json(std::string menu, user_bind bind, std::string prefix)
{
   //std::cout << menu << std::endl;

   if (std::empty(menu))
      return {};

   json j;
   std::stringstream ss;
   ss << menu;
   ss >> j;

   std::stack<std::vector<std::pair<std::string, json>>> st;
   st.push({{prefix, j}});
   std::stack<std::string> hashes;
   do {
      while (!st.top().back().second["sub"].is_null()) {
         auto const vec =
            st.top().back().second["sub"].get<std::vector<json>>();
         std::vector<std::pair<std::string, json>> tmp;
         int i = 0;
         for (auto const& o : vec) {
            auto str = st.top().back().first;
            str.append(".");
            str.append(to_str(i++, 2, '0'));
            tmp.push_back({std::move(str), o});
         }
         st.push(std::move(tmp));
      }

      json jcmd;
      jcmd["cmd"] = "create_group";
      jcmd["from"] = bind;
      jcmd["hash"] = st.top().back().first;
      jcmd["info"] = group_info {"Atibaia", "Centro"};

      hashes.push(jcmd.dump());

      st.top().pop_back();

      if (!std::empty(st.top()))
         continue;
      
      st.pop();

      auto const name2 = st.top().back().second["name"].get<std::string>();
      std::cout << name2 << std::endl;

      st.top().pop_back();

      if (std::empty(st.top()))
         st.pop();

   } while (std::size(st) != 1);

   return hashes;
}


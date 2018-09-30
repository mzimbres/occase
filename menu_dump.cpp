#include <iostream>

#include "menu_parser.hpp"

void test2()
{
   auto const menu = gen_location_menu();
   //std::cout << menu.dump(4) << std::endl;

   std::cout << menu.dump() << std::endl;

   auto const cmds = gen_create_groups(menu);
   for (auto const& o : cmds)
      std::cout << o << std::endl;

   auto j_infos = gen_group_info(menu);
   std::cout << j_infos.dump(4) << std::endl;

   auto cmds2 = gen_join_groups(menu, {"Marcelo"});
   for (auto const& o : cmds2)
      std::cout << o << std::endl;

   auto cmds3 = get_hashes(menu);
   for (auto const& o : cmds3)
      std::cout << o << " ";
   std::cout << std::endl;
}

json gen_location_menu1()
{
   std::vector<json> j3 =
   { {{"hash", ""}, {"name", "Atibaia"},   {"sub", {}}}
   , {{"hash", ""}, {"name", "Sao Paulo"}, {"sub", {}}}
   };

   json j;
   j["name"] = "SP";
   j["sub_desc"] = "Cidades";
   j["sub"] = j3;

   return j;
}

void test1()
{
   auto menu = gen_location_menu1();
   //std::cout << menu.dump(4) << std::endl;

   std::cout << menu.dump() << std::endl;

   auto const cmds = gen_create_groups(menu);
   for (auto const& o : cmds)
      std::cout << o << std::endl;
}

json gen_location_menu3()
{
   std::vector<json> j1 =
   { {{"hash", ""}, {"sub", {}}, {"name", "Centro"}}};

   std::vector<json> j2 =
   { {{"hash", ""}, {"sub", {}}, {"name", "Vila Leopoldina"}, } };

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

void test3()
{
   auto menu = gen_location_menu3();
   //std::cout << menu.dump(4) << std::endl;

   auto const cmds = gen_create_groups(menu);
   for (auto const& o : cmds)
      std::cout << o << std::endl;
}

json gen_location_menu0()
{
   json j;
   j["name"] = "SP";
   j["sub_desc"] = "Cidades";
   j["sub"] = {};
   j["hash"] = "";

   return j;
}

void test0()
{
   auto menu = gen_location_menu0();
   //std::cout << menu.dump(4) << std::endl;

   auto const cmds = gen_create_groups(menu);
   for (auto const& o : cmds)
      std::cout << o << std::endl;
}

int main()
{
   test0();
   std::cout << "__________________________________________" << std::endl;
   test1();
   std::cout << "__________________________________________" << std::endl;
   test2();
   std::cout << "__________________________________________" << std::endl;
   test3();
}


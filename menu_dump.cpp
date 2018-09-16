#include <iostream>

#include "menu_parser.hpp"

void test2()
{
   auto menu = gen_location_menu();
   //std::cout << menu.dump(4) << std::endl;

   auto hash_patches = gen_hash_patches(menu);

   menu = menu.patch(hash_patches);
   std::cout << menu.dump() << std::endl;

   auto const cmds = gen_create_groups(menu, {"Marcelo", "Criatura", -1});
   for (auto const& o : cmds)
      std::cout << o << std::endl;

   gen_group_info(menu);
}

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

void test1()
{
   auto menu = gen_location_menu1();
   //std::cout << menu.dump(4) << std::endl;

   auto hash_patches = gen_hash_patches(menu);
   //json j = hash_patches;
   //std::cout << j.dump(4) << std::endl;

   menu = menu.patch(hash_patches);
   std::cout << menu.dump() << std::endl;

   auto const cmds = gen_create_groups(menu, {"Marcelo", "Criatura", -1});
   for (auto const& o : cmds)
      std::cout << o << std::endl;
}

json gen_location_menu3()
{
   std::vector<json> j1 =
   { {{"status", "off"}, {"hash", ""}, {"sub", {}}, {"name", "Centro"}}};

   std::vector<json> j2 =
   { {{"status", "off"}, {"hash", ""}, {"sub", {}}, {"name", "Vila Leopoldina"}, } };

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

   auto hash_patches = gen_hash_patches(menu);
   json j = hash_patches;
   std::cout << j.dump() << std::endl;

   menu = menu.patch(hash_patches);
   std::cout << menu.dump() << std::endl;

   auto const cmds = gen_create_groups(menu, {"Marcelo", "Criatura", -1});
   for (auto const& o : cmds)
      std::cout << o << std::endl;
}

json gen_location_menu0()
{
   json j;
   j["name"] = "SP";
   j["sub_desc"] = "Cidades";
   j["sub"] = {};
   j["status"] = "off";
   j["hash"] = "";

   return j;
}

void test0()
{
   auto menu = gen_location_menu0();
   //std::cout << menu.dump(4) << std::endl;

   auto hash_patches = gen_hash_patches(menu);
   //json j = hash_patches;
   //std::cout << j.dump(4) << std::endl;

   menu = menu.patch(hash_patches);
   std::cout << menu.dump() << std::endl;

   auto const cmds = gen_create_groups(menu, {"Marcelo", "Criatura", -1});
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


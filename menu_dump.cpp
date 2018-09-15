#include <iostream>

#include "menu_parser.hpp"

void test()
{
   auto menu = gen_location_menu();
   std::cout << menu.dump(4) << std::endl;
   std::cout << "__________________________________________" << std::endl;

   auto patches = json_patches(menu);
   menu = menu.patch(patches);

   std::cout << menu.dump(4) << std::endl;
   std::cout << "__________________________________________" << std::endl;

   user_bind bind {"Marcelo", "Criatura", -1};
   auto cg_cmds = parse_menu_json(menu, bind, "00");
   while (!std::empty(cg_cmds)) {
      std::cout << cg_cmds.top() << std::endl;
      cg_cmds.pop();
   }
}

void test1()
{
   std::cout << "__________________________________________" << std::endl;
   auto menu = gen_location_menu1();
   std::cout << menu.dump(4) << std::endl;
   std::cout << "__________________________________________" << std::endl;

   auto patches = json_patches(menu);
   json j = patches;
   std::cout << j.dump(4) << std::endl;
   std::cout << "__________________________________________" << std::endl;
   menu = menu.patch(patches);
   std::cout << menu.dump(4) << std::endl;
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
   std::cout << "__________________________________________" << std::endl;
   auto menu = gen_location_menu0();
   std::cout << menu.dump(4) << std::endl;
   std::cout << "__________________________________________" << std::endl;

   auto patches = json_patches(menu);
   json j = patches;
   std::cout << j.dump(4) << std::endl;
   std::cout << "__________________________________________" << std::endl;

   menu = menu.patch(patches);
   std::cout << menu.dump(4) << std::endl;
}

int main()
{
   test0();
   test1();
   test();
}

#include <iostream>

#include "menu_parser.hpp"

int main()
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


#include <iostream>

#include "menu_parser.hpp"

int main()
{
   auto menu = gen_menu_json(4);
   std::cout << menu << std::endl;
   std::cout << "__________________________________________" << std::endl;

   user_bind bind {"Marcelo", "Criatura", -1};
   auto cg_cmds = parse_menu_json(menu, bind, "00");
   while (!std::empty(cg_cmds)) {
      std::cout << cg_cmds.top() << std::endl;
      cg_cmds.pop();
   }
}


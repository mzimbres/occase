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

   json j_patch = R"([
     { "op": "replace", "path": "/sub/0/sub/0/hash", "value": "00.02.03" },
     { "op": "replace", "path": "/sub/0/sub/1/hash", "value": "00.02.03" }
   ])"_json;

   // apply the patch
   json jj = json::parse(menu);
   json result = jj.patch(j_patch);
   std::cout << result.dump(4) << std::endl;

   json_patches(menu);
}


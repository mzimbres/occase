#pragma once

#include <stack>
#include <vector>
#include <string>
#include <memory>
#include <stdexcept>

#include "config.hpp"
#include "json_utils.hpp"
#include "menu_parser.hpp"

template <class Mgr>
class client_session;

// Creates groups.

class client_mgr_cg {
private:
   using client_type = client_session<client_mgr_cg>;
   std::string expected;
   std::stack<std::string> cmds;

public:
   client_mgr_cg(std::string exp)
   : expected(exp)
   {

      auto const menu = gen_location_menu();
      auto const cmds_ = gen_create_groups(menu);

      //std::cout << "aaaaaaaaaa " << std::endl;
      if (std::empty(cmds_))
         throw std::runtime_error("client_mgr_cg: Stack is empty.");

      for (auto const& o : cmds_)
         cmds.push(std::move(o));
   }
   ~client_mgr_cg();

   int on_read(json j, std::shared_ptr<client_type> s);
   int on_closed(boost::system::error_code ec);
   int on_handshake(std::shared_ptr<client_type> s);
   int on_connect() const noexcept { return 1;}
};


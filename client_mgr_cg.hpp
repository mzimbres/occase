#pragma once

#include <stack>
#include <string>
#include <memory>
#include <stdexcept>

#include "config.hpp"
#include "json_utils.hpp"

template <class Mgr>
class client_session;

// Creates groups.

class client_mgr_cg {
private:
   using client_type = client_session<client_mgr_cg>;
   std::string expected;
   std::stack<std::string> cmds;

public:
   client_mgr_cg(std::string exp, std::stack<std::string> cmds_)
   : expected(exp)
   , cmds(std::move(cmds_))
   {
      //std::cout << "aaaaaaaaaa " << std::endl;
      if (std::empty(cmds))
         throw std::runtime_error("client_mgr_cg: Stack is empty.");
   }
   ~client_mgr_cg();

   int on_read(json j, std::shared_ptr<client_type> s);
   int on_closed(boost::system::error_code ec);
   int on_handshake(std::shared_ptr<client_type> s);
   int on_connect() const noexcept { return 1;}
};


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

class client_mgr_sim {
private:
   using client_type = client_session<client_mgr_sim>;
   user_bind bind;
   std::string expected;
   std::stack<std::string> cmds;
   std::stack<std::string> hashes;

   void send_group_msg(std::shared_ptr<client_type> s);

public:
   client_mgr_sim( std::string exp
                 , std::stack<std::string> cmds_
                 , std::stack<std::string> hashes_
                 , user_bind bind_)
   : bind(bind_)
   , expected(exp)
   , cmds(std::move(cmds_))
   , hashes(std::move(hashes_))
   {
      if (std::empty(cmds) || std::empty(hashes))
         throw std::runtime_error("client_mgr_sim: Stack is empty.");
   }
   int on_read(json j, std::shared_ptr<client_type> s);
   int on_closed(boost::system::error_code ec);
   int on_handshake(std::shared_ptr<client_type> s);
   int on_connect() const noexcept { return 1;}
};


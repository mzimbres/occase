#pragma once

#include <set>
#include <queue>
#include <string>
#include <memory>

#include "config.hpp"
#include "json_utils.hpp"

namespace rt
{

template <class Mgr>
class client_session;

// Sends some register commands with wrong or invalid fields that should
// cause the server to shutdown the connection. In the end a valid
// register command is send.
//
// TODO: What should happen if we send a new register command when there
//       is an ongoing register transaction?

struct cmgr_register_cf {
   std::string user;
   std::string expected;
   int on_read_ret;
};

class client_mgr_register {
private:
   using client_type = client_session<client_mgr_register>;
   cmgr_register_cf op;

public:
   using options_type = cmgr_register_cf;
   client_mgr_register(options_type op_) : op(op_) { }

   int on_read(std::string msg, std::shared_ptr<client_type> s);
   int on_closed(boost::system::error_code ec) const noexcept
      { return -1; }
   int on_handshake(std::shared_ptr<client_type> s);
   int on_connect() const noexcept
      { return 1;}
   auto get_user() const {return op.user;}
};

//___________________________________________________________________

// Sends registers that will be dropped.

class client_mgr_register_typo {
private:
   using client_type = client_session<client_mgr_register_typo>;
   std::string cmd;

public:
   using options_type = std::string;
   client_mgr_register_typo(std::string cmd_) : cmd(cmd_) { }
   auto on_read( std::string msg
              , std::shared_ptr<client_type> s) const
      { throw std::runtime_error("register_typo::on_read"); return -1; }
   auto on_closed(boost::system::error_code ec) const noexcept
      { return -1; }
   int on_handshake(std::shared_ptr<client_type> s) const;
   auto on_connect() const noexcept
      { return 1;}
   auto get_user() const noexcept
      { return "ccccccc"; }
};

}


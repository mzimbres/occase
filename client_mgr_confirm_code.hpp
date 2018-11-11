#pragma once

#include <string>
#include <memory>

#include "config.hpp"
#include "json_utils.hpp"

namespace rt
{

template <class Mgr>
class client_session;

// Performs a register followed by a code confirmation.

struct cmgr_code_op {
   std::string user;
   std::string expected;
   std::string code;
   int end_ret;
};

class client_mgr_code {
public:
   using options_type = cmgr_code_op;

private:
   using client_type = client_session<client_mgr_code>;
   options_type op;

public:
   client_mgr_code(options_type op_) : op(op_) { }
   int on_read(std::string msg, std::shared_ptr<client_type> s);
   int on_closed(boost::system::error_code ec) const noexcept
      {return -1;};
   int on_handshake(std::shared_ptr<client_type> s);
   int on_connect() const noexcept
      { return 1;}
   auto get_user() const
      { return op.user;}
};

// Tries to authenticate a session with the user bind provided on the
// code commands.

struct cmgr_auth_op {
   std::string user;
   std::string expected;
};

class client_mgr_auth {
public:
   using options_type = cmgr_auth_op;
private:
   using client_type = client_session<client_mgr_auth>;
   options_type op;

public:
   client_mgr_auth(options_type op_) : op(op_) {}
   int on_read(std::string msg, std::shared_ptr<client_type> s);
   auto on_closed(boost::system::error_code ec)
      { throw std::runtime_error("mgr_auth::on_closed"); return -1; };
   int on_handshake(std::shared_ptr<client_type> s);
   auto on_connect() const noexcept
      { return 1;}
   auto get_user() const
      {return "dddddd";}
};

}


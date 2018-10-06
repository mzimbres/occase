#pragma once

#include <set>
#include <queue>
#include <string>
#include <memory>

#include "config.hpp"
#include "json_utils.hpp"

template <class Mgr>
class client_session;

// Sends some login commands with wrong or invalid fields that should
// cause the server to shutdown the connection. In the end a valid
// login command is send.
//
// TODO: What should happen if we send a new login command when there
//       is an ongoing login transaction?

struct cmgr_login_cf {
   std::string user;
   std::string expected;
   int on_read_ret;
};

class client_mgr_login {
private:
   using client_type = client_session<client_mgr_login>;
   cmgr_login_cf op;

public:
   using options_type = cmgr_login_cf;
   client_mgr_login(options_type op);
   int on_read(std::string msg, std::shared_ptr<client_type> s);
   int on_closed(boost::system::error_code ec) { return -1; }
   int on_handshake(std::shared_ptr<client_type> s);
   int on_connect() const noexcept { return 1;}
   auto get_user() const {return op.user;}
};

//___________________________________________________________________

// Sends logins that will be dropped.

class client_mgr_login_typo {
private:
   using client_type = client_session<client_mgr_login_typo>;
   std::string cmd;

public:
   using options_type = std::string;
   client_mgr_login_typo(std::string cmd);
   int on_read(std::string msg, std::shared_ptr<client_type> s);
   int on_closed(boost::system::error_code ec);
   int on_handshake(std::shared_ptr<client_type> s);
   int on_connect() const noexcept { return 1;}
   auto get_user() const {return "ccccccc";}
};


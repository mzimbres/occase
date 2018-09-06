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

class client_mgr_login {
private:
   using client_type = client_session<client_mgr_login>;
   std::string tel;

public:
   client_mgr_login(std::string tel_);
   int on_read(json j, std::shared_ptr<client_type> s);
   int on_closed(boost::system::error_code ec);
   int on_handshake(std::shared_ptr<client_type> s);
};

//___________________________________________________________________

// Sends logins that will be dropped.

class client_mgr_login1 {
private:
   using client_type = client_session<client_mgr_login1>;
   std::string cmd;

public:
   client_mgr_login1(std::string cmd);
   int on_read(json j, std::shared_ptr<client_type> s);
   int on_closed(boost::system::error_code ec);
   int on_handshake(std::shared_ptr<client_type> s);
};


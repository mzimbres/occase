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
// TODO: Rename login to register.
// TODO: What should happen if we send a new login command when there
//       is an ongoing login transaction?

class client_mgr_login {
private:
   using client_type = client_session<client_mgr_login>;
   std::string tel;

   // login
   int number_of_ok_logins = 1;
   int number_of_dropped_logins = 3;

   void send_ok_login(std::shared_ptr<client_type> s);
   void send_dropped_login(std::shared_ptr<client_type> s);

public:
   client_mgr_login(std::string tel_);
   int on_read(json j, std::shared_ptr<client_type> s);
   int on_closed(boost::system::error_code ec);
   int on_handshake(std::shared_ptr<client_type> s);
};


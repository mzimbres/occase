#pragma once

#include <set>
#include <queue>
#include <string>
#include <memory>

#include "config.hpp"
#include "json_utils.hpp"

template <class Mgr>
class client_session;

class client_mgr_login {
private:
   using client_type = client_session<client_mgr_login>;
   std::string tel;
   std::queue<std::string> msg_queue;

   // login
   int number_of_ok_logins = 1;
   int number_of_dropped_logins = 3;

   void send_ok_login(std::shared_ptr<client_type> s);
   void send_dropped_login(std::shared_ptr<client_type> s);
   int on_login_ack(json j, std::shared_ptr<client_type> s);
   int on_ok_login_ack(json j, std::shared_ptr<client_type> s);

   void send_msg(std::string msg, std::shared_ptr<client_type> s);

public:
   client_mgr_login(std::string tel_);
   int on_read(json j, std::shared_ptr<client_type> s);
   int on_fail_read(boost::system::error_code ec);
   int on_write(std::shared_ptr<client_type> s);
   int on_handshake(std::shared_ptr<client_type> s);
};


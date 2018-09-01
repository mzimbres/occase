#pragma once

#include <set>
#include <queue>
#include <string>
#include <memory>

#include "config.hpp"
#include "json_utils.hpp"

template <class Mgr>
class client_session;

class client_mgr_sms {
private:
   using client_type = client_session<client_mgr_sms>;
   std::queue<std::string> msg_queue;
   std::string tel;

   // login
   int number_of_ok_logins = 1;
   void send_ok_login(std::shared_ptr<client_type> s);
   int on_login_ack(json j, std::shared_ptr<client_type> s);

   // sms
   void send_ok_sms_confirmation(std::shared_ptr<client_type> s);
   int on_sms_confirmation_ack(json j, std::shared_ptr<client_type> s);

   void send_msg(std::string msg, std::shared_ptr<client_type> s);

public:
   client_mgr_sms(std::string tel_);
   user_bind bind;
   int on_read(json j, std::shared_ptr<client_type> s);
   int on_fail_read(boost::system::error_code ec);
   int on_write(std::shared_ptr<client_type> s);
   int on_handshake(std::shared_ptr<client_type> s);
};


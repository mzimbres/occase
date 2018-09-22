#pragma once

#include <string>
#include <memory>

#include "config.hpp"
#include "json_utils.hpp"

template <class Mgr>
class client_session;

// Performs a login followed by an sms confirmation.

class client_mgr_sms {
private:
   using client_type = client_session<client_mgr_sms>;
   std::string tel;
   std::string expected;
   std::string sms;

public:
   client_mgr_sms( std::string tel_
                 , std::string expected_
                 , std::string sms_)
   : tel(tel_)
   , expected(expected_)
   , sms(sms_)
   { }

   user_bind bind;
   int on_read(json j, std::shared_ptr<client_type> s);
   int on_closed(boost::system::error_code ec) {return 1;};
   int on_handshake(std::shared_ptr<client_type> s);
   int on_connect() const noexcept { return 1;}
};

// Tries to authenticate a session with the user bind provided on the
// sms commands.

class client_mgr_auth {
private:
   using client_type = client_session<client_mgr_auth>;
   user_bind bind;
   std::string expected;

public:
   client_mgr_auth(user_bind bind_, std::string exp)
   : bind(bind_)
   , expected(exp) {}
   int on_read(json j, std::shared_ptr<client_type> s);
   int on_closed(boost::system::error_code ec);
   int on_handshake(std::shared_ptr<client_type> s);
   int on_connect() const noexcept { return 1;}
};


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

public:
   client_mgr_sms(std::string tel_);
   user_bind bind;
   int on_read(json j, std::shared_ptr<client_type> s);
   int on_closed(boost::system::error_code ec) {return 1;};
   int on_handshake(std::shared_ptr<client_type> s);
};


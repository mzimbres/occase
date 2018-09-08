#pragma once

#include <string>
#include <memory>

#include "config.hpp"
#include "json_utils.hpp"

template <class Mgr>
class client_session;

// Creates groups.

class client_mgr_cg {
private:
   using client_type = client_session<client_mgr_cg>;
   user_bind bind;
   std::string expected;

public:
   client_mgr_cg(user_bind bind_, std::string exp)
   : bind(bind_)
   , expected(exp)
   {}
   int on_read(json j, std::shared_ptr<client_type> s);
   int on_closed(boost::system::error_code ec);
   int on_handshake(std::shared_ptr<client_type> s);
};


#pragma once

#include <memory>

#include "json_utils.hpp"

#include "config.hpp"

template <class Mgr>
class client_session;

// Tests if the server drops a session that does not proceed with
// authentication.

class client_mgr_accept_timer {
private:
   using client_type = client_session<client_mgr_accept_timer>;

public:
   int on_read(json j, std::shared_ptr<client_type> s);
   int on_closed(boost::system::error_code ec);
   int on_write(std::shared_ptr<client_type> s);
   int on_handshake(std::shared_ptr<client_type> s) { return 1;}
};


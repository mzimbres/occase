#pragma once

#include <memory>

#include "json_utils.hpp"

#include "config.hpp"

template <class Mgr>
class client_session;

// Tests if the server drops a session that does not proceed with
// authentication.
//
// TODO: Consider flooding the server with connections. Should it rely
// on timeouts or should it check that the IP address has an ongoing
// connection and block it?

class client_mgr_accept_timer {
private:
   using client_type = client_session<client_mgr_accept_timer>;

   int number_of_reconnects = 5;

public:
   int on_read(json j, std::shared_ptr<client_type> s);
   int on_closed(boost::system::error_code ec);
   int on_write(std::shared_ptr<client_type> s);
   int on_handshake(std::shared_ptr<client_type> s);
};


#pragma once

#include <memory>

#include "json_utils.hpp"

#include "config.hpp"

template <class Mgr>
class client_session;

// Tests if the server drops a session that does not proceed with
// authentication.
// TODO: Add a timer here after which we should emit an error if the
// server do not drop the connection.

class client_mgr_on_connect_timer {
private:
   using client_type = client_session<client_mgr_on_connect_timer>;

public:
   ~client_mgr_on_connect_timer()
   {
      //std::cout << "Bye1 bye1." << std::endl;
   }
   int on_read(json j, std::shared_ptr<client_type> s) const;
   int on_closed(boost::system::error_code ec) const;
   int on_write(std::shared_ptr<client_type> s) const;
   int on_handshake(std::shared_ptr<client_type> s) const;
   int on_connect() const;
};

class client_mgr_accept_timer {
private:
   using client_type = client_session<client_mgr_accept_timer>;

public:
   ~client_mgr_accept_timer()
   {
      //std::cout << "Bye2 bye2." << std::endl;
   }

   int on_read(json j, std::shared_ptr<client_type> s);
   int on_closed(boost::system::error_code ec);
   int on_write(std::shared_ptr<client_type> s);
   int on_handshake(std::shared_ptr<client_type> s) { return -1;}
   int on_connect() const noexcept { return 1;}
};


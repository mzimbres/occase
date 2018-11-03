#pragma once

#include <memory>

#include "json_utils.hpp"

#include "config.hpp"

namespace rt
{

template <class Mgr>
class client_session;

// Tests if the server drops a session that does not proceed with
// authentication.
// TODO: Add a timer here after which we should emit an error if the
// server do not drop the connection.

struct cmgr_handshake_op {
   std::string user;
};

class cmgr_handshake_tm {
private:
   using client_type = client_session<cmgr_handshake_tm>;

public:
   using options_type = cmgr_handshake_op;
   cmgr_handshake_tm(cmgr_handshake_op)
   {
      //std::cout << "Hi." << std::endl;
   }
   ~cmgr_handshake_tm()
   {
      //std::cout << "Bye1 bye1." << std::endl;
   }
   int on_read(std::string msg, std::shared_ptr<client_type> s) const;
   int on_closed(boost::system::error_code ec) const;
   int on_write(std::shared_ptr<client_type> s) const;
   int on_handshake(std::shared_ptr<client_type> s) const;
   int on_connect() const;
   auto get_user() const {return "aaaaaa";}
};

class client_mgr_accept_timer {
private:
   using client_type = client_session<client_mgr_accept_timer>;

public:
   using options_type = cmgr_handshake_op;
   client_mgr_accept_timer(cmgr_handshake_op)
   {
      //std::cout << "Hi." << std::endl;
   }
   ~client_mgr_accept_timer()
   {
      //std::cout << "Bye2 bye2." << std::endl;
   }

   int on_read(std::string msg, std::shared_ptr<client_type> s);
   int on_closed(boost::system::error_code ec);
   int on_write(std::shared_ptr<client_type> s);
   int on_handshake(std::shared_ptr<client_type> s) { return -1;}
   int on_connect() const noexcept { return 1;}
   auto get_user() const {return "aaaaaa";}
};

}


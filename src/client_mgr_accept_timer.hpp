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
// server does not drop the connection.

struct cmgr_handshake_op {
   std::string user;
};

struct cmgr_handshake_tm {
   using client_type = client_session<cmgr_handshake_tm>;
   using options_type = cmgr_handshake_op;
   cmgr_handshake_tm(cmgr_handshake_op) noexcept { }
   auto on_read(std::string msg, std::shared_ptr<client_type> s) const 
      { throw std::runtime_error("Error."); return 1; }
   auto on_closed(boost::system::error_code ec) const 
      { throw std::runtime_error("Error."); return 1; }
   auto on_write(std::shared_ptr<client_type> s) const
      { throw std::runtime_error("Error."); return 1; }
   auto on_handshake(std::shared_ptr<client_type> s) const 
      { throw std::runtime_error("Error."); return 1; }
   auto on_connect() const noexcept
      { return -1; }
   auto get_user() const noexcept
      {return "Dummy";}
};

class client_mgr_accept_timer {
private:
   using client_type = client_session<client_mgr_accept_timer>;

public:
   using options_type = cmgr_handshake_op;
   client_mgr_accept_timer(cmgr_handshake_op) noexcept { }
   auto on_read(std::string msg, std::shared_ptr<client_type> s)
      { throw std::runtime_error("accept_timer::on_read"); return -1; }
   auto on_closed(boost::system::error_code ec) const noexcept
      { return -1; }
   auto on_write(std::shared_ptr<client_type> s)
      { throw std::runtime_error("accept_timer::on_write"); return 1; }
   auto on_handshake(std::shared_ptr<client_type> s)
      { return -1;}
   auto on_connect() const noexcept
      { return 1;}
   auto get_user() const noexcept
      {return "aaaaaa";}
};

}


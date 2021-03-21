#pragma once

#include "net.hpp"

namespace occase
{

class worker;

class acceptor_mgr {
private:
   net::ip::tcp::acceptor acceptor_;

   void do_accept(worker& w, ssl::context& ctx);

   void on_accept(
      worker& w,
      ssl::context& ctx,
      boost::system::error_code ec,
      tcp::socket peer);

public:
   acceptor_mgr(net::io_context& ioc);

   auto is_open() const noexcept
      { return acceptor_.is_open(); }

   void run( worker& w
           , ssl::context& ctx
           , unsigned short port
           , int max_listen_connections);

   void shutdown();
};

} // occase

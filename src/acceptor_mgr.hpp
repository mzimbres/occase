#pragma once

#include <boost/asio.hpp>

#include "net.hpp"

namespace rt
{

class worker;

class acceptor_mgr {
private:
   net::ip::tcp::acceptor acceptor;

   void do_accept(worker& w);
   void on_accept( worker& w
                 , boost::system::error_code ec
                 , net::ip::tcp::socket peer);

public:
   acceptor_mgr(net::io_context& ioc);
   void run( worker& w
           , unsigned short port
           , int max_listen_connections);
   void shutdown();
};

}



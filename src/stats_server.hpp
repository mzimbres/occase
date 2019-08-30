#pragma once

#include "config.hpp"

namespace rt
{

class worker;

struct stats_server_cfg {
   std::string port;
};

class stats_server {
private:
   tcp::acceptor acceptor_;

   void do_accept(worker const& w);
   void on_accept( worker const& w
                 , boost::system::error_code ec
                 , net::ip::tcp::socket peer);

public:
   stats_server(stats_server_cfg const& cfg, net::io_context& ioc);
   void run(worker const& w);
   void shutdown();
};

}


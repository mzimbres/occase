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
   tcp::socket socket_;
   worker const& worker_;

   void do_accept();
   void on_accept(beast::error_code const& ec);

public:
   stats_server( stats_server_cfg const& cfg
               , worker const& w
               , int i
               , net::io_context& ioc);

   void run();
   void shutdown();
};

}


#pragma once

#include "config.hpp"

namespace rt
{

class worker_arena;

struct stats_server_cfg {
   std::string port;
};

class stats_server {
private:
   tcp::acceptor acceptor_;
   tcp::socket socket_;
   std::vector<std::shared_ptr<worker_arena>> const& warenas_;

   void do_accept();
   void on_accept(beast::error_code const& ec);

public:
   stats_server( stats_server_cfg const& cfg
               , std::vector<std::shared_ptr<worker_arena>> const& warenas
               , net::io_context& ioc);

   void run();
   void shutdown();
};

}


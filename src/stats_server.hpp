#pragma once

#include "config.hpp"

namespace rt
{

class worker;

struct server_stats {
   int number_of_sessions;
   int worker_post_queue_size;
   int worker_reg_queue_size;
   int worker_login_queue_size;
   int db_post_queue_size;
   int db_chat_queue_size;
};

std::ostream& operator<<(std::ostream& os, server_stats const& stats);

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


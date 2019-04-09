#include "config.hpp"

struct stats_server {
   tcp::acceptor acceptor_;
   tcp::socket socket_;

   stats_server( char const* addr, unsigned short port
               , net::io_context& ioc);

   void run();
};


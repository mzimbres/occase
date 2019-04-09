#include "config.hpp"

namespace rt {

class stats_server {
private:
   int id_;
   tcp::acceptor acceptor_;
   tcp::socket socket_;

   void do_accept();
   void on_accept(beast::error_code const& ec);

public:
   stats_server( std::string const& addr
               , std::string const& port
               , int id
               , net::io_context& ioc);

   void run();
   void shutdown();
};

}


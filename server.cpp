#include <stack>
#include <memory>
#include <string>
#include <chrono>
#include <vector>
#include <thread>
#include <cstdlib>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <functional>
#include <unordered_map>
#include <unordered_set>

#include <boost/beast/core.hpp>
#include <boost/asio/bind_executor.hpp>
#include <boost/asio/strand.hpp>

#include "config.hpp"
#include "user.hpp"
#include "server_session.hpp"

void fail(boost::system::error_code ec, char const* what)
{
   std::cerr << what << ": " << ec.message() << "\n";
}

// Accepts incoming connections and launches the sessions
class listener : public std::enable_shared_from_this<listener> {
private:
   tcp::acceptor acceptor;
   tcp::socket socket;
   std::shared_ptr<server_data> sd;

public:
   listener( boost::asio::io_context& ioc
           , tcp::endpoint endpoint
           , std::shared_ptr<server_data> sd_)
   : acceptor(ioc)
   , socket(ioc)
   , sd(sd_)
   {
      boost::system::error_code ec;

      // Open the acceptor
      acceptor.open(endpoint.protocol(), ec);
      if (ec) {
         fail(ec, "open");
         return;
      }

      // Allow address reuse
      acceptor.set_option(boost::asio::socket_base::reuse_address(true));
      if (ec) {
         fail(ec, "set_option");
         return;
      }

      // Bind to the server address
      acceptor.bind(endpoint, ec);
      if (ec) {
         fail(ec, "bind");
         return;
      }

      // Start listening for connections
      acceptor.listen(
            boost::asio::socket_base::max_listen_connections, ec);
      if (ec) {
         fail(ec, "listen");
         return;
      }
   }

   // Start accepting incoming connections
   void run()
   {
      if (!acceptor.is_open())
         return;

      do_accept();
   }

   void do_accept()
   {
      auto handler = [p = shared_from_this()](auto ec)
      { p->on_accept(ec); };

      acceptor.async_accept(socket, handler);
   }

   void on_accept(boost::system::error_code ec)
   {
      if (ec) {
         fail(ec, "accept");
      } else {
         std::make_shared<server_session>( std::move(socket)
                                         , sd)->run();
      }

      // Accept another connection
      do_accept();
   }
};

int main(int argc, char* argv[])
{
   auto const address = boost::asio::ip::make_address("127.0.0.1");
   auto const port = static_cast<unsigned short>(8080);
   auto const threads = 1;

   boost::asio::io_context ioc {threads};

   
   auto users_size = 100;
   auto groups_size = 100;

   auto sd = std::make_shared<server_data>(users_size, groups_size);

   auto lt = std::make_shared<listener>( ioc , tcp::endpoint {address, port}
                                       , sd);

   lt->run();
   ioc.run();
   return EXIT_SUCCESS;
}


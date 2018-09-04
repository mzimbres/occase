#include <memory>

#include <boost/asio/signal_set.hpp>

#include "listener.hpp"

struct signal_handler {
   boost::asio::io_context& ioc;
   void operator()(boost::system::error_code const&, int)
   {
      // TODO: Investigate if we can simply destroy the work object
      // and let the io_context exit cleanly. That would mean we have
      // to close each socket connection from each active user, that
      // may not be feasible. Other clean ups how however important,
      // like pushing some data to the database.

      // Stop the io_context. This will cause run() to return
      // immediately, eventually destroying the io_context and all of
      // the sockets in it.
      std::cout << "\nI am cleaning up some operations." << std::endl;
      ioc.stop();
   }
};

int main(int argc, char* argv[])
{
   auto const address = boost::asio::ip::make_address("127.0.0.1");
   auto const port = static_cast<unsigned short>(8080);
   auto const users_size = 10;
   auto const groups_size = 10;

   boost::asio::io_context ioc {1};

   auto sd = std::make_shared<server_mgr>(users_size, groups_size);

   std::make_shared<listener>( ioc , tcp::endpoint {address, port}
                             , sd)->run();

   // Capture SIGINT and SIGTERM to perform a clean shutdown
   boost::asio::signal_set signals(ioc, SIGINT, SIGTERM);
   signals.async_wait(signal_handler {ioc});

   ioc.run();
   return EXIT_SUCCESS;
}


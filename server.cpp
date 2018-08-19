#include <memory>

#include <boost/asio/bind_executor.hpp>

#include "listener.hpp"

int main(int argc, char* argv[])
{
   auto const address = boost::asio::ip::make_address("127.0.0.1");
   auto const port = static_cast<unsigned short>(8080);
   auto const threads = 1;
   auto const users_size = 10;
   auto const groups_size = 10;

   boost::asio::io_context ioc {threads};
   
   auto sd = std::make_shared<server_data>(users_size, groups_size);

   std::make_shared<listener>( ioc , tcp::endpoint {address, port}
                             , sd)->run();

   ioc.run();
   return EXIT_SUCCESS;
}


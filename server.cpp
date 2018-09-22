#include <memory>
#include <string>
#include <iterator>

#include <boost/asio/signal_set.hpp>
#include <boost/program_options/options_description.hpp>
#include <boost/program_options/parsers.hpp>
#include <boost/program_options/variables_map.hpp>

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

namespace po = boost::program_options;

int main(int argc, char* argv[])
{
   try {
      std::string ip;
      unsigned short port;
      po::options_description desc("Allowed options");
      desc.add_options()
         ("help", "produce help message")
         ( "port,p"
         , po::value<unsigned short>(&port)->default_value(8080)
         , "Port")
         ("ip-address,p"
         , po::value<std::string>(&ip)->default_value("127.0.0.1")
         , "Port")
         ;

      po::variables_map vm;        
      po::store(po::parse_command_line(argc, argv, desc), vm);
      po::notify(vm);    

      if (vm.count("help")) {
         std::cout << desc << "\n";
         return 0;
      }

      auto const address = boost::asio::ip::make_address(ip);

      boost::asio::io_context ioc {1};

      auto sd = std::make_shared<server_mgr>(users_size);

      std::make_shared<listener>( ioc , tcp::endpoint {address, port}
                                , sd)->run();

      // Capture SIGINT and SIGTERM to perform a clean shutdown
      boost::asio::signal_set signals(ioc, SIGINT, SIGTERM);
      signals.async_wait(signal_handler {ioc});

      ioc.run();
      return 0;
   } catch(std::exception const& e) {
       std::cerr << "error: " << e.what() << "\n";
       return 1;
   }
}


#include <memory>
#include <string>
#include <chrono>
#include <iterator>

#include <boost/asio/signal_set.hpp>
#include <boost/program_options/options_description.hpp>
#include <boost/program_options/parsers.hpp>
#include <boost/program_options/variables_map.hpp>

#include "listener.hpp"

struct signal_handler {
   boost::asio::io_context& ioc;
   std::shared_ptr<listener> lst;
   std::shared_ptr<server_mgr> sm;

   void operator()(boost::system::error_code const&, int)
   {
      std::cout << "\nBeginning the shutdown operations ..." << std::endl;

      // This function is called when the program receives one of the
      // installed signals. The listener stop functions will continue
      // with other necessary clean up operations.
      lst->stop();
   }
};

namespace po = boost::program_options;

struct server_op {
   std::string ip;
   unsigned short port;
   int n_init_users;
   int on_acc_timeout;
   int sms_timeout;

   auto session_config() const noexcept
   {
      return server_session_config
      { std::chrono::seconds {on_acc_timeout}
      , std::chrono::seconds {sms_timeout}};
   }
};

int main(int argc, char* argv[])
{
   try {
      server_op op;
      po::options_description desc("Options");
      desc.add_options()
         ("help,h", "produce help message")
         ( "port,p"
         , po::value<unsigned short>(&op.port)->default_value(8080)
         , "Server port.")
         ("ip-address,d"
         , po::value<std::string>(&op.ip)->default_value("127.0.0.1")
         , "Server ip address.")
         ("init-users,u"
         , po::value<int>(&op.n_init_users)->default_value(100)
         , "Initial number of users.")
         ("sms-timeout,s"
         , po::value<int>(&op.sms_timeout)->default_value(2)
         , "SMS confirmation timeout in seconds.")
         ("accept-timeout,a"
         , po::value<int>(&op.on_acc_timeout)->default_value(2)
         , "On accept timeout in seconds.")
      ;

      po::variables_map vm;        
      po::store(po::parse_command_line(argc, argv, desc), vm);
      po::notify(vm);    

      if (vm.count("help")) {
         std::cout << desc << "\n";
         return 0;
      }

      auto const address = boost::asio::ip::make_address(op.ip);

      boost::asio::io_context ioc {1};

      auto sm = std::make_shared<server_mgr>(op.n_init_users);

      auto lst =
         std::make_shared<listener>( ioc 
                                   , tcp::endpoint {address, op.port}
                                   , sm, op.session_config());
      lst->run();

      // Capture SIGINT and SIGTERM to perform a clean shutdown
      boost::asio::signal_set signals(ioc, SIGINT, SIGTERM);
      signals.async_wait(signal_handler {ioc, lst, sm});

      ioc.run();
      return 0;
   } catch(std::exception const& e) {
       std::cerr << "error: " << e.what() << "\n";
       return 1;
   }
}


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
   int auth_timeout;
   int sms_timeout;
   int handshake_timeout;
   int pong_timeout;
   int number_of_threads;
   int close_frame_timeout;

   auto get_timeouts() const noexcept
   {
      return session_timeouts
      { std::chrono::seconds {auth_timeout}
      , std::chrono::seconds {sms_timeout}
      , std::chrono::seconds {handshake_timeout}
      , std::chrono::seconds {pong_timeout}
      , std::chrono::seconds {close_frame_timeout}
      };
   }
};

int main(int argc, char* argv[])
{
   try {
      server_op op;
      po::options_description desc("Options");
      desc.add_options()
         ("help,h", "Produces help message")
         ( "port,p"
         , po::value<unsigned short>(&op.port)->default_value(8080)
         , "Server listening port."
         )
         ("ip,d"
         , po::value<std::string>(&op.ip)->default_value("127.0.0.1")
         , "Server ip address."
         )
         ("sms-timeout,s"
         , po::value<int>(&op.sms_timeout)->default_value(2)
         , "SMS confirmation timeout in seconds."
         )
         ("auth-timeout,a"
         , po::value<int>(&op.auth_timeout)->default_value(2)
         , "Authetication timeout in seconds. Fired after the websocket "
           "handshake completes. Used also by the login "
           "command for clients registering for the first time."
         )
         ("handshake-timeout,k"
         , po::value<int>(&op.handshake_timeout)->default_value(2)
         , "Handshake timeout in seconds. If the websocket handshake lasts "
           "more than that the socket is shutdown and closed."
         )
         ("pong-timeout,r"
         , po::value<int>(&op.pong_timeout)->default_value(2)
         , "Pong timeout in seconds. This is the time the client has to "
           "reply a ping frame sent by the server. If a pong is received "
           "on time a new ping is sent on timer expiration."
         )
         ("close-frame-timeout,e"
         , po::value<int>(&op.close_frame_timeout)->default_value(2)
         , "The time we are willing to wait for a reply of a sent close frame."
         )

         ("number-of-threads,c"
         , po::value<int>(&op.number_of_threads)->default_value(1)
         , "The number of threads for the io_context. This is usually "
           "the number of processor cores."
         )
      ;

      po::variables_map vm;        
      po::store(po::parse_command_line(argc, argv, desc), vm);
      po::notify(vm);    

      if (vm.count("help")) {
         std::cout << desc << "\n";
         return 0;
      }

      if (op.number_of_threads < 1) {
         std::cout << "Number of threads must be at least 1."
                   << std::endl;
         return 0;
      }

      auto const address = boost::asio::ip::make_address(op.ip);

      boost::asio::io_context ioc {op.number_of_threads};

      auto sm = std::make_shared<server_mgr>(op.get_timeouts());

      auto lst =
         std::make_shared<listener>( ioc 
                                   , tcp::endpoint {address, op.port}
                                   , sm);
      lst->run();

      boost::asio::signal_set signals(ioc, SIGINT, SIGTERM);
      signals.async_wait(signal_handler {ioc, lst, sm});

      std::vector<std::thread> threads;
      threads.reserve(op.number_of_threads - 1);

      for (auto i = 0; i < op.number_of_threads - 1; ++i)
          threads.emplace_back([&ioc] { ioc.run(); });

      ioc.run();
      return 0;
   } catch(std::exception const& e) {
       std::cerr << "error: " << e.what() << "\n";
       return 1;
   }
}


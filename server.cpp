#include <memory>
#include <string>
#include <chrono>
#include <iterator>

#include <boost/asio/signal_set.hpp>
#include <boost/program_options/options_description.hpp>
#include <boost/program_options/parsers.hpp>
#include <boost/program_options/variables_map.hpp>

#include "listener.hpp"

namespace po = boost::program_options;

server_op get_server_op(int argc, char* argv[])
{
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
   , po::value<int>(&op.mgr.sms_timeout)->default_value(2)
   , "SMS confirmation timeout in seconds."
   )
   ("auth-timeout,a"
   , po::value<int>(&op.mgr.auth_timeout)->default_value(2)
   , "Authetication timeout in seconds. Fired after the websocket "
     "handshake completes. Used also by the login "
     "command for clients registering for the first time."
   )
   ("handshake-timeout,k"
   , po::value<int>(&op.mgr.handshake_timeout)->default_value(2)
   , "Handshake timeout in seconds. If the websocket handshake lasts "
     "more than that the socket is shutdown and closed."
   )
   ("pong-timeout,r"
   , po::value<int>(&op.mgr.pong_timeout)->default_value(2)
   , "Pong timeout in seconds. This is the time the client has to "
     "reply a ping frame sent by the server. If a pong is received "
     "on time a new ping is sent on timer expiration."
   )
   ("close-frame-timeout,e"
   , po::value<int>(&op.mgr.close_frame_timeout)->default_value(2)
   , "The time we are willing to wait for a reply of a sent close frame."
   )

   ("redis-address"
   , po::value<std::string>(&op.mgr.redis_address)->default_value("127.0.0.1")
   , "Address of redis server."
   )
   ("redis-port"
   , po::value<std::string>(&op.mgr.redis_port)->default_value("6379")
   , "Port where redis server is listening."
   )
   ("redis-group-channel"
   , po::value<std::string>(&op.mgr.redis_group_channel)->default_value("group_msgs")
   , "The name of the redis channel where group messages will be broadcasted."
   )
   ;

   po::variables_map vm;        
   po::store(po::parse_command_line(argc, argv, desc), vm);
   po::notify(vm);    

   if (vm.count("help")) {
      std::cout << desc << "\n";
      op.help = true;
   }

   return op;
}

int main(int argc, char* argv[])
{
   try {
      auto const op = get_server_op(argc, argv);
      if (op.help)
         return 0;

      boost::asio::io_context ioc {1};
      listener lst {op, ioc};
      lst.run();
      ioc.run();

      return 0;
   } catch (std::exception const& e) {
       std::cerr << "Error: " << e.what() << "\n";
       return 1;
   }
}


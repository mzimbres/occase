#include <thread>
#include <memory>
#include <string>
#include <chrono>
#include <iterator>

#include <boost/asio/signal_set.hpp>
#include <boost/program_options/options_description.hpp>
#include <boost/program_options/parsers.hpp>
#include <boost/program_options/variables_map.hpp>

#include "listener.hpp"

using namespace rt;

namespace po = boost::program_options;

std::vector<server_op> get_server_op(int argc, char* argv[])
{
   int instances = 1;
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
   ("instances"
   , po::value<int>(&instances)->default_value(1)
   , "The number of share-nothing server instances, each"
     " one consuming one thread and having its own io_context"
     " object. They will continuously occupy the ports"
     " begining at --port."
   )
   ("code-timeout,s"
   , po::value<int>(&op.mgr.code_timeout)->default_value(2)
   , "Code confirmation timeout in seconds."
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
   ("redis-menu-key"
   , po::value<std::string>(&op.mgr.redis_menu_key)->default_value("menu")
   , "Redis key holding the menu."
   )
   ("redis-msg-prefix"
   , po::value<std::string>(&op.mgr.redis_msg_prefix)->default_value("msg")
   , "That prefix that will be incorporated in the keys that hold"
     " user messages."
   )
   ;

   po::variables_map vm;        
   po::store(po::parse_command_line(argc, argv, desc), vm);
   po::notify(vm);    

   if (vm.count("help")) {
      std::cout << desc << "\n";
      return {};
   }

   std::vector<server_op> ops;
   for (auto i = 0; i < instances; ++i) {
      ops.push_back(op);
      ops.back().port += i;
   }

   return ops;
}

struct instance {
   server_op op;
   void operator()() const
   {
      boost::asio::io_context ioc {1};
      server_mgr mgr {op.mgr, ioc};
      listener lst {op, mgr};
      lst.run();
      ioc.run();
   }
};

int main(int argc, char* argv[])
{
   try {
      auto const ops = get_server_op(argc, argv);
      if (std::empty(ops))
         return 0;

      std::vector<std::thread> instances;
      for (unsigned i = 0; i < std::size(ops) - 1; ++i)
         instances.push_back(std::thread {instance{ops[i]}});

      instance inst {ops.back()};
      inst();

      for (auto& o : instances)
         o.join();

      return 0;
   } catch (std::exception const& e) {
       std::cerr << "Error: " << e.what() << "\n";
       return 1;
   }
}


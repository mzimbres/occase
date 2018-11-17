#include <thread>
#include <memory>
#include <string>
#include <chrono>
#include <iterator>
#include <algorithm>

#include <boost/asio/signal_set.hpp>
#include <boost/program_options/options_description.hpp>
#include <boost/program_options/parsers.hpp>
#include <boost/program_options/variables_map.hpp>

#include "listener.hpp"
#include "mgr_arena.hpp"

using namespace rt;

namespace po = boost::program_options;

struct config {
   bool help = false;
   server_mgr_cf mgr;
   int workers;
   std::vector<listener_cf> lts_cf;
};

auto get_server_op(int argc, char* argv[])
{
   unsigned short port;
   std::string ip;
   config cf;
   po::options_description desc("Options");
   desc.add_options()
   ("help,h", "Produces help message")
   ( "port,p"
   , po::value<unsigned short>(&port)->default_value(8080)
   , "Server listening port."
   )
   ("ip,d"
   , po::value<std::string>(&ip)->default_value("127.0.0.1")
   , "Server ip address."
   )
   ("workers,w"
   , po::value<int>(&cf.workers)->default_value(1)
   , "The number of worker threads, each"
     " one consuming one thread and having its own io_context."
     " For example, in a CPU with 8 cores this number should lie"
     " around 5."
   )
   ("code-timeout,s"
   , po::value<int>(&cf.mgr.code_timeout)->default_value(2)
   , "Code confirmation timeout in seconds."
   )
   ("auth-timeout,a"
   , po::value<int>(&cf.mgr.auth_timeout)->default_value(2)
   , "Authetication timeout in seconds. Fired after the websocket "
     "handshake completes. Used also by the login "
     "command for clients registering for the first time."
   )
   ("handshake-timeout,k"
   , po::value<int>(&cf.mgr.handshake_timeout)->default_value(2)
   , "Handshake timeout in seconds. If the websocket handshake lasts "
     "more than that the socket is shutdown and closed."
   )
   ("pong-timeout,r"
   , po::value<int>(&cf.mgr.pong_timeout)->default_value(2)
   , "Pong timeout in seconds. This is the time the client has to "
     "reply a ping frame sent by the server. If a pong is received "
     "on time a new ping is sent on timer expiration."
   )
   ("close-frame-timeout,e"
   , po::value<int>(&cf.mgr.close_frame_timeout)->default_value(2)
   , "The time we are willing to wait for a reply of a sent close frame."
   )

   ("redis-address"
   , po::value<std::string>(&cf.mgr.redis_address)->default_value("127.0.0.1")
   , "Address of redis server."
   )
   ("redis-port"
   , po::value<std::string>(&cf.mgr.redis_port)->default_value("6379")
   , "Port where redis server is listening."
   )
   ("redis-group-channel"
   , po::value<std::string>(&cf.mgr.redis_group_channel)->default_value("group_msgs")
   , "The name of the redis channel where group messages will be broadcasted."
   )
   ("redis-menu-key"
   , po::value<std::string>(&cf.mgr.redis_menu_key)->default_value("menu")
   , "Redis key holding the menu."
   )
   ("redis-msg-prefix"
   , po::value<std::string>(&cf.mgr.redis_msg_prefix)->default_value("msg")
   , "That prefix that will be incorporated in the keys that hold"
     " user messages."
   )
   ;

   po::variables_map vm;        
   po::store(po::parse_command_line(argc, argv, desc), vm);
   po::notify(vm);    

   if (vm.count("help")) {
      std::cout << desc << "\n";
      return config {true};
   }

   cf.lts_cf.push_back({ip, port});
   //cf.lts_cf.port = static_cast<unsigned short>(port);

   return cf;
}

struct acceptor_pool {
   boost::asio::io_context ioc {1};
   boost::asio::signal_set signals;
   listener lst;

   acceptor_pool( std::vector<listener_cf> const& lts_cf
                , std::vector< std::unique_ptr<mgr_arena>
                              > const& arenas)
   : signals(ioc, SIGINT, SIGTERM)
   , lst {lts_cf.front(), arenas, ioc}
   { run(); }

   void run()
   {
      lst.run();
      auto const sigh = [this](auto ec, auto n)
      {
         // TODO: Verify ec here.
         std::cout << "\nBeginning the shutdown operations ..."
                   << std::endl;
         lst.shutdown();
      };

      signals.async_wait(sigh);
      ioc.run();
   }
};

int main(int argc, char* argv[])
{
   try {
      auto const cf = get_server_op(argc, argv);
      if (cf.help)
         return 0;

      auto const generator = [mgr = cf.mgr]()
      { return std::make_unique<mgr_arena>(mgr); };

      std::vector<std::unique_ptr<mgr_arena>> arenas;
      std::generate_n(std::back_inserter(arenas), cf.workers, generator);

      acceptor_pool acc_pool {cf.lts_cf, arenas};
      acc_pool.run();

      for (auto& o : arenas)
         o->join();

      return 0;
   } catch (std::exception const& e) {
       std::cerr << "Error: " << e.what() << "\n";
       return 1;
   }
}


#include <thread>
#include <memory>
#include <string>
#include <chrono>
#include <iterator>
#include <algorithm>

#include <boost/program_options/options_description.hpp>
#include <boost/program_options/parsers.hpp>
#include <boost/program_options/variables_map.hpp>

#include "acceptor_arena.hpp"
#include "server_mgr.hpp"

using namespace rt;

namespace po = boost::program_options;

struct config {
   bool help = false;
   server_mgr_cf mgr;
   int workers;
   unsigned short port;

   int auth_timeout;
   int code_timeout;
   int handshake_timeout;
   int pong_timeout;
   int close_frame_timeout;

   auto get_timeouts() const noexcept
   {
      return session_timeouts
      { std::chrono::seconds {auth_timeout}
      , std::chrono::seconds {code_timeout}
      , std::chrono::seconds {handshake_timeout}
      , std::chrono::seconds {pong_timeout}
      , std::chrono::seconds {close_frame_timeout}
      };
   }

};

auto get_server_op(int argc, char* argv[])
{
   std::vector<std::string> ips;
   config cf;
   int conn_retry_interval = 500;
   po::options_description desc("Options");
   desc.add_options()
   ("help,h", "Produces help message")
   ( "port,p"
   , po::value<unsigned short>(&cf.port)->default_value(8080)
   , "Server listening port."
   )
   ("workers,w"
   , po::value<int>(&cf.workers)->default_value(1)
   , "The number of worker threads, each"
     " one consuming one thread and having its own io_context."
     " For example, in a CPU with 8 cores this number should lie"
     " around 5."
   )
   ("code-timeout,s"
   , po::value<int>(&cf.code_timeout)->default_value(2)
   , "Code confirmation timeout in seconds."
   )
   ("auth-timeout,a"
   , po::value<int>(&cf.auth_timeout)->default_value(2)
   , "Authetication timeout in seconds. Fired after the websocket "
     "handshake completes. Used also by the login "
     "command for clients registering for the first time."
   )
   ("handshake-timeout,k"
   , po::value<int>(&cf.handshake_timeout)->default_value(2)
   , "Handshake timeout in seconds. If the websocket handshake lasts "
     "more than that the socket is shutdown and closed."
   )
   ("pong-timeout,r"
   , po::value<int>(&cf.pong_timeout)->default_value(2)
   , "Pong timeout in seconds. This is the time the client has to "
     "reply a ping frame sent by the server. If a pong is received "
     "on time a new ping is sent on timer expiration."
   )
   ("close-frame-timeout,e"
   , po::value<int>(&cf.close_frame_timeout)->default_value(2)
   , "The time we are willing to wait for a reply of a sent close frame."
   )

   ("redis-address"
   , po::value<std::string>(&cf.mgr.redis_cf.sessions.host)->
       default_value("127.0.0.1")
   , "Address of redis server."
   )
   ("redis-port"
   , po::value<std::string>(&cf.mgr.redis_cf.sessions.port)->
       default_value("6379")
   , "Port where redis server is listening."
   )
   ("redis-menu-channel"
   , po::value<std::string>(&cf.mgr.redis_cf.nms.menu_channel)->
        default_value("menu_channel")
   , "The name of the redis channel where group messages will be broadcasted."
   )
   ("redis-menu-key"
   , po::value<std::string>(&cf.mgr.redis_cf.nms.menu_key)->default_value("menu")
   , "Redis key holding the menu."
   )
   ("redis-msg-prefix"
   , po::value<std::string>(&cf.mgr.redis_cf.nms.msg_prefix)->default_value("msg")
   , "That prefix that will be incorporated in the keys that hold"
     " user messages."
   )
   ("redis-conn-retry-interval"
   , po::value<int>(&conn_retry_interval)->default_value(500)
   , "Time in milliseconds the redis session should wait before trying "
     "to reconnect to redis."
   )
   ;

   po::variables_map vm;        
   po::store(po::parse_command_line(argc, argv, desc), vm);
   po::notify(vm);    

   if (vm.count("help")) {
      std::cout << desc << "\n";
      return config {true};
   }

   cf.mgr.redis_cf.nms.msg_prefix += ":";
   cf.mgr.redis_cf.nms.notify_prefix += cf.mgr.redis_cf.nms.msg_prefix;
   cf.mgr.redis_cf.sessions.conn_retry_interval =
      std::chrono::milliseconds{conn_retry_interval};
   cf.mgr.timeouts = cf.get_timeouts();
   return cf;
}

int main(int argc, char* argv[])
{
   try {
      auto const cf = get_server_op(argc, argv);
      if (cf.help)
         return 0;

      std::vector<std::shared_ptr<server_mgr>> workers;

      std::generate_n( std::back_inserter(workers)
                     , cf.workers
                     , [&cf]()
                       { return std::make_shared<server_mgr>(cf.mgr); });

      std::vector<std::thread> threads;
      for (auto o : workers)
         threads.emplace_back(std::thread {[o](){ o->run();}});

      acceptor_arena acc {cf.port, workers};
      acc.run();

      for (auto& o : threads)
         o.join();

      return 0;
   } catch (std::exception const& e) {
       std::cerr << "Error: " << e.what() << "\n";
       return 1;
   }
}


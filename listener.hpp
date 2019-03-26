#pragma once

#include <memory>
#include <vector>
#include <thread>

#include <boost/asio.hpp>
#include <boost/asio/signal_set.hpp>

#include "config.hpp"
#include "worker.hpp"
#include "server_session.hpp"

namespace rt
{

struct listener_cfg {
   bool help = false;
   bool log_on_stderr = false;
   server_cf mgr;
   int number_of_workers;
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

class listener {
private:
   net::io_context ioc {1};
   net::signal_set signals;
   net::ip::tcp::acceptor acceptor;
   std::vector<std::shared_ptr<worker>> workers;
   std::vector<std::thread> threads;
   long long next = 0;

   void do_accept();
   void on_accept( boost::system::error_code ec
                 , net::ip::tcp::socket peer);

public:
   listener(listener_cfg const& cg);
   void run();
   void shutdown();
};

}


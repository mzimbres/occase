#pragma once

#include <memory>
#include <vector>
#include <thread>

#include <boost/asio.hpp>
#include <boost/asio/signal_set.hpp>

#include "config.hpp"
#include "worker.hpp"
#include "stats_server.hpp"
#include "worker_session.hpp"

namespace rt
{

struct listener_cfg {
   bool help = false;
   bool log_on_stderr = false;
   bool daemonize = false;
   worker_cfg worker;
   stats_server_cfg stats;
   int number_of_workers;
   unsigned short port;
   std::string pidfile;

   int login_timeout;
   int handshake_timeout;
   int pong_timeout;
   int close_frame_timeout;

   std::string loglevel;

   auto get_timeouts() const noexcept
   {
      return ws_ss_timeouts
      { std::chrono::seconds {login_timeout}
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
   worker worker_;
   stats_server sserver;

   void do_accept();
   void on_accept( boost::system::error_code ec
                 , net::ip::tcp::socket peer);

public:
   listener(listener_cfg const& cg, int i);
   void run();
   void on_signal(boost::system::error_code const& ec, int n);
};

}


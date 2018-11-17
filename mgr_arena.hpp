#pragma once

#include <thread>

#include <boost/asio.hpp>

#include "server_mgr.hpp"

namespace rt
{

struct mgr_arena {
   boost::asio::io_context ioc {1};
   boost::asio::signal_set signals;
   server_mgr mgr;
   std::thread thread;

   mgr_arena(server_mgr_cf const& cf)
   : signals(ioc, SIGINT, SIGTERM)
   , mgr {cf, ioc}
   { }

   void run()
   {
      auto const sig_handler = [this](auto ec, auto n)
      {
         // TODO: Verify ec here.
         std::cout << "\nBeginning the shutdown operations ..."
                   << std::endl;

         mgr.shutdown();
      };

      signals.async_wait(sig_handler);

      thread = std::thread {[this](){ioc.run();}};
   }

   void join() { thread.join(); }
};

}


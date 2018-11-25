#pragma once

#include <vector>
#include <iostream>

#include <boost/asio.hpp>
#include <boost/asio/signal_set.hpp>

#include "server_mgr.hpp"
#include "listener.hpp"

namespace rt
{

// TODO: Add ipv6 support.

struct acceptor_arena {
   boost::asio::io_context ioc {1};
   boost::asio::signal_set signals;
   listener lst;

   acceptor_arena( unsigned short port
                 , std::vector<std::shared_ptr<server_mgr>> const& workers)
   : signals(ioc, SIGINT, SIGTERM)
   , lst { {boost::asio::ip::tcp::v4(), port}, workers, ioc}
   {
      run();
   }

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

}


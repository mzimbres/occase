#pragma once

#include <vector>
#include <iostream>

#include <boost/asio.hpp>
#include <boost/asio/signal_set.hpp>

#include "mgr_arena.hpp"
#include "listener.hpp"

namespace rt
{

struct acceptors {
   boost::asio::io_context ioc {1};
   boost::asio::signal_set signals;
   listener lst;

   acceptors( std::vector<listener_cf> const& lts_cf
            , std::vector<std::unique_ptr<mgr_arena>> const& arenas)
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

}


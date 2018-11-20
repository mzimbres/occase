#include "mgr_arena.hpp"

namespace rt
{

mgr_arena::mgr_arena(server_mgr_cf const& cf)
: signals(ioc, SIGINT, SIGTERM)
, mgr {cf, ioc}
, socket {ioc}
{
   run();
}

void mgr_arena::run()
{
   auto const sig_handler = [this](auto ec, auto n)
   {
      // TODO: Verify ec here.
      std::cout << "\nBeginning the shutdown operations ..."
                << std::endl;

      mgr.shutdown();
   };

   signals.async_wait(sig_handler);

   auto const handler = [this]
   {
      try {
         ioc.run();
      } catch (std::exception const& e) {
        std::cout << e.what() << std::endl;
      }
   };

   thread = std::thread {handler};
}

}


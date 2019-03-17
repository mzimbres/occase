#include "listener.hpp"

#include <iostream>

#include "server_session.hpp"
#include "server_mgr.hpp"
#include "server_mgr.hpp"

namespace rt
{

listener::listener( net::ip::tcp::endpoint const& endpoint
                  , std::vector< std::shared_ptr<server_mgr>
                               > const& workers_)
: signals(ioc, SIGINT, SIGTERM)
, acceptor(ioc, endpoint)
, workers(workers_)
{
   std::clog << "Binding server to " << acceptor.local_endpoint()
             << std::endl;
}

void listener::run()
{
   auto const handler = [this](auto ec, auto n)
   {
      // TODO: Verify ec here.
      std::clog << "\nBeginning the shutdown operations ..."
                << std::endl;

      shutdown();
   };

   signals.async_wait(handler);

   if (!acceptor.is_open())
      return;

   do_accept();

   ioc.run();
}

void listener::do_accept()
{
   auto const n = next % std::size(workers);
   acceptor.async_accept( workers[n]->get_io_context()
                        , [this](auto const& ec, auto socket)
                          { on_accept(ec, std::move(socket)); });
}

void listener::on_accept( boost::system::error_code ec
                        , net::ip::tcp::socket peer)
{
   if (ec) {
      if (ec == net::error::operation_aborted) {
         std::cout << "Stopping accepting connections ..." << std::endl;
         return;
      }

      fail(ec, "accept");
      return;
   }

   auto const n = next % std::size(workers);
   std::make_shared< server_session
                   >( std::move(peer), workers[n])->accept();
   ++next;

   do_accept();
}

}


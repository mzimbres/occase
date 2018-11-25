#include "listener.hpp"

#include <iostream>

#include "server_session.hpp"
#include "server_mgr.hpp"
#include "server_mgr.hpp"

namespace
{

void fail(boost::system::error_code ec, char const* what)
{
   std::cerr << what << ": " << ec.message() << "\n";
}

}

namespace rt
{

listener::listener( net::ip::tcp::endpoint const& endpoint
                  , std::vector< std::shared_ptr<server_mgr>
                               > const& workers_
                  , net::io_context& ioc)
: acceptor(ioc, endpoint)
, workers(workers_)
{
   std::cout << "Binding server to " << acceptor.local_endpoint()
             << std::endl;
}

void listener::run()
{
   if (!acceptor.is_open())
      return;

   do_accept();
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


#include "listener.hpp"

#include <iostream>

#include "server_session.hpp"
#include "server_mgr.hpp"
#include "mgr_arena.hpp"

namespace
{

void fail(boost::system::error_code ec, char const* what)
{
   std::cerr << what << ": " << ec.message() << "\n";
}

}

namespace rt
{

listener::listener( boost::asio::ip::tcp::endpoint const& endpoint
                  , std::vector<std::unique_ptr<mgr_arena>> const& arenas_
                  , boost::asio::io_context& ioc)
: acceptor(ioc, endpoint)
, arenas(arenas_)
{
   for (auto const& o : arenas)
      sockets.emplace_back(o->get_io_context());
}

void listener::run()
{
   if (!acceptor.is_open())
      return;

   do_accept();
}

void listener::do_accept()
{
   auto handler = [this](auto ec)
   {
      on_accept(ec);
   };

   auto const n = next % std::size(sockets);
   acceptor.async_accept(sockets[n], handler);
}

void listener::on_accept(boost::system::error_code ec)
{
   if (ec) {
      if (ec == boost::asio::error::operation_aborted) {
         std::cout << "Stopping accepting connections ..." << std::endl;
         return;
      }

      fail(ec, "accept");
      return;
   }

   auto const n = next % std::size(sockets);
   std::make_shared< server_session
                   >( std::move(sockets[n])
                    , arenas[n]->get_mgr())->accept();
   ++next;

   do_accept();
}

}


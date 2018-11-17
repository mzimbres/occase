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

listener::listener( listener_cf op
                  , std::vector<std::unique_ptr<mgr_arena>> const& arenas_
                  , boost::asio::io_context& ioc)
: acceptor(ioc)
, arenas(arenas_)
{
   for (auto const& o : arenas)
      sockets.emplace_back(o->get_io_context());

   auto const address = boost::asio::ip::make_address(op.ip);
   tcp::endpoint endpoint {address, op.port};

   boost::system::error_code ec;
   acceptor.open(endpoint.protocol(), ec);
   if (ec) {
      fail(ec, "open");
      return;
   }

   acceptor.set_option(boost::asio::socket_base::reuse_address(true));
   if (ec) {
      fail(ec, "set_option");
      return;
   }

   acceptor.bind(endpoint, ec);
   if (ec) {
      fail(ec, "bind");
      return;
   }

   //std::cout << "max_listen_connections: "
   //          << boost::asio::socket_base::max_listen_connections << std::endl;

   acceptor.listen(boost::asio::socket_base::max_listen_connections, ec);
   if (ec) {
      fail(ec, "listen");
      return;
   }
}

listener::~listener()
{
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


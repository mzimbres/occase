#include "listener.hpp"

#include <iostream>

//#include <boost/beast/core.hpp>

#include "server_session.hpp"

namespace
{

void fail(boost::system::error_code ec, char const* what)
{
   std::cerr << what << ": " << ec.message() << "\n";
}

}

listener::listener( boost::asio::io_context& ioc
                  , tcp::endpoint endpoint
                  , std::shared_ptr<server_mgr> mgr_
                  , std::shared_ptr<const server_session_timeouts> timeouts)
: acceptor(ioc)
, socket(ioc)
, mgr(mgr_)
, timeouts(timeouts)
{
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

   acceptor.listen(
         boost::asio::socket_base::max_listen_connections, ec);
   if (ec) {
      fail(ec, "listen");
      return;
   }
}

void listener::run()
{
   if (!acceptor.is_open())
      return;

   do_accept();
}

void listener::do_accept()
{
   auto handler = [p = shared_from_this()](auto ec)
   { p->on_accept(ec); };

   acceptor.async_accept(socket, handler);
}

void listener::on_accept(boost::system::error_code ec)
{
   if (ec) {
      if (ec == boost::asio::error::operation_aborted) {
         std::cout << "Stopping accepting connections ..." << std::endl;
         // An accepted that has been canceled is to be interpreted
         // for now as a shutdown operation so that we have to perform
         // some further cleanup.
         mgr->shutdown();
         return;
      }

      fail(ec, "accept");
      return;
   }

   std::make_shared<server_session>( std::move(socket)
                                   , session_shared {mgr, timeouts}
                                   )->do_accept();

   do_accept();
}


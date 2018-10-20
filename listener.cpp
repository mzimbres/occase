#include "listener.hpp"

#include <iostream>

#include "server_session.hpp"
#include "server_mgr.hpp"

namespace
{

void fail(boost::system::error_code ec, char const* what)
{
   std::cerr << what << ": " << ec.message() << "\n";
}

}

listener::listener(server_op op, boost::asio::io_context& ioc)
: signals(ioc, SIGINT, SIGTERM)
, acceptor(ioc)
, socket(ioc)
, mgr(op.mgr)
, session_stats_timer(ioc)
{
   auto const shandler = [this](auto ec, auto n)
   {
      // TODO: Verify ec here.
      std::cout << "\nBeginning the shutdown operations ..."
                << std::endl;

      acceptor.cancel();
      session_stats_timer.cancel();
      mgr.shutdown();
   };

   signals.async_wait(shandler);

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

   acceptor.listen(
         boost::asio::socket_base::max_listen_connections, ec);
   if (ec) {
      fail(ec, "listen");
      return;
   }
}

listener::~listener()
{
}

void listener::do_stats_logger()
{
   session_stats_timer.expires_after(std::chrono::seconds{1});

   auto handler = [this](auto ec)
   {
      if (ec) {
         if (ec == boost::asio::error::operation_aborted)
            return;

         return;
      }
      
      std::cout << "Current number of sessions: "
                << mgr.get_stats().number_of_sessions
                << std::endl;

      do_stats_logger();
   };

   session_stats_timer.async_wait(handler);
}

void listener::run()
{
   if (!acceptor.is_open())
      return;

   do_accept();
   do_stats_logger();
}

void listener::do_accept()
{
   auto handler = [this](auto ec)
   {
      on_accept(ec);
   };

   acceptor.async_accept(socket, handler);
}

void listener::on_accept(boost::system::error_code ec)
{
   if (ec) {
      if (ec == boost::asio::error::operation_aborted) {
         std::cout << "Stopping accepting connections ..." << std::endl;
         // An accept that has been canceled is to be interpreted for
         // now as a shutdown operation so that we have to perform
         // some further cleanup.
         mgr.shutdown();
         return;
      }

      fail(ec, "accept");
      return;
   }

   std::make_shared<server_session>(std::move(socket), mgr)->accept();

   do_accept();
}


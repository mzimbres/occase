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
                  , std::shared_ptr<server_data> sd_)
: acceptor(ioc)
, socket(ioc)
, sd(sd_)
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
      fail(ec, "accept");
   } else {
      std::make_shared<server_session>( std::move(socket)
                                      , sd)->run();
   }

   do_accept();
}


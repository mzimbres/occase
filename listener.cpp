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
   auto const n = next % std::size(arenas);
   acceptor.async_accept( arenas[n]->get_mgr().get_socket()
                        , [this](auto const& ec)
                          { on_accept(ec); });
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

   auto const n = next % std::size(arenas);
   std::make_shared< server_session
                   >( std::move(arenas[n]->get_mgr().get_socket())
                    , arenas[n]->get_mgr())->accept();
   ++next;

   do_accept();
}

}


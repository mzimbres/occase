#include <deque>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string_view>

#include <boost/asio.hpp>
#include <boost/asio/bind_executor.hpp>
#include <boost/asio/ip/tcp.hpp>

#include "config.hpp"
#include "redis_session.hpp"

namespace rt::redis
{

void session::on_resolve( boost::system::error_code const& ec
                        , net::ip::tcp::resolver::results_type results)
{
   if (ec)
      return fail(ec, "resolve");

   auto const handler = [this](auto ec, auto iter)
      { on_connect(ec, iter); };

   net::async_connect(socket, results, handler);
}

void session::run()
{
   auto const handler = [this](auto ec, auto res)
      { on_resolve(ec, res); };

   resolver.async_resolve(cf.host, cf.port, handler );
}

void session::send(std::string msg)
{
   auto const is_empty = std::empty(msg_queue);

   if (is_empty || std::size(msg_queue) == 1)
      msg_queue.push(std::move(msg));
   else
      msg_queue.back() += msg; // Uses pipeline.

   if (is_empty && socket.is_open())
      net::async_write( socket, net::buffer(msg_queue.front())
                      , [this](auto ec, auto n) {on_write(ec, n);});
}

void session::close()
{
   boost::system::error_code ec;
   socket.shutdown(net::ip::tcp::socket::shutdown_send, ec);
   //if (ec)
   //   fail(ec, "redis-close");

   socket.close(ec);
   //if (ec)
   //   fail(ec, "redis-close");

   timer.cancel();
}

void session::start_reading_resp()
{
   buffer.res.clear();

   auto const handler = [this](auto const& ec)
      { on_resp(ec); };

   async_read_resp(socket, &buffer, handler);
}

void session::on_connect( boost::system::error_code const& ec
                        , net::ip::tcp::endpoint const& endpoint)
{
   if (ec) {
      // TODO: Traverse all endpoints if this one is not available. If
      // none is available should we continue? What to do with
      // messages that have no been sent to redis.
      return;
   }

   start_reading_resp();

   // Calls user callback to inform a successfull connect to redis.
   // He may wish to start sending some command.
   on_conn_handler();

   // Consumes any messages that have been eventually posted while the
   // connection was not established.
   if (!std::empty(msg_queue))
      net::async_write( socket, net::buffer(msg_queue.front())
                      , [this](auto ec, auto n)
                        { on_write(ec, n); });
}

void session::on_resp(boost::system::error_code const& ec)
{
   if (ec == net::error::eof) {
      // Redis has cleanly closed the connection. We try a reconnect.
      timer.expires_after(cf.conn_retry_interval);

      auto const handler = [this](auto ec)
      {
         if (ec) {
            if (ec == net::error::operation_aborted) {
               // The timer has been canceled. Probably somebody
               // shutting down the application while we are trying to
               // reconnect.
               //
               // TODO: How should we do with messages that have not
               // been sent to the database.
               return;
            }

            fail(ec, "Redis reconnect handler");
            return;
         }

         // Given that the peer has shutdown the connection (I think)
         // we do not need to call shutdown.
         //socket.shutdown(net::ip::tcp::socket::shutdown_both, ec);
         socket.close(ec);
         assert(!socket.is_open());

         // Instead of simply trying to reconnect I will run the
         // resolver again.
         //
         // TODO: Store the endpoints object obtained from the first
         // call to resolve and try only to reconnect instead. Or will
         // resolving again can be useful?
         std::cout << "Trying to reconnect." << std::endl;
         run();
      };

      timer.async_wait(handler);
      return;
   }

   msg_handler(ec, buffer.res);
   if (!std::empty(msg_queue))
      msg_queue.pop();

   if (!ec && socket.is_open()) {
      start_reading_resp();
      if (!std::empty(msg_queue))
         net::async_write( socket, net::buffer(msg_queue.front())
                         , [this](auto ec, auto n) { on_write(ec, n); });
   }
}

void session::on_write(boost::system::error_code ec, std::size_t n)
{
   if (ec) {
      fail(ec, "on_write");
      return;
   }
}

}


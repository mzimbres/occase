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

void session::on_resolve( boost::system::error_code ec
                        , net::ip::tcp::resolver::results_type results)
{
   if (ec)
      return fail(ec, "resolve");

   net::async_connect( socket, results
                     , [this](auto ec, auto Iterator)
                       { on_connect(ec, Iterator); });
}

void session::run()
{
   resolver.async_resolve( cf.host, cf.port
                         , [this](auto ec, auto res)
                           { on_resolve(ec, res); });
}

void session::send(req_data req)
{
   auto const is_empty = std::empty(write_queue);
   write_queue.push(std::move(req));

   if (is_empty && socket.is_open())
      net::async_write( socket, net::buffer(write_queue.front().msg)
                      , [this](auto ec, auto n)
                        {on_write(ec, n);});
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
}

void session::start_reading_resp()
{
   buffer.res.clear();
   async_read_resp( socket, &buffer
                  , [this](auto const& ec)
                    { on_resp(ec); });
}

void session::on_connect( boost::system::error_code ec
                        , net::ip::tcp::endpoint const& endpoint)
{
   if (ec) {
      fail(ec, "on_connect");
      return;
   }

   //net::ip::tcp::no_delay option(true);
   //socket.set_option(option);

   start_reading_resp();

   // Consumes any messages that have been eventually posted while the
   // connection was not established.
   if (!std::empty(write_queue))
      net::async_write( socket, net::buffer(write_queue.front().msg)
                      , [this](auto ec, auto n)
                        { on_write(ec, n); });
}

void session::on_resp(boost::system::error_code const& ec)
{
   if (std::empty(write_queue)) {
      msg_handler(ec, buffer.res, {});
   } else {
      msg_handler(ec, buffer.res, write_queue.front());
      write_queue.pop();
   }

   if (!ec && socket.is_open()) {
      start_reading_resp();
      if (!std::empty(write_queue))
         net::async_write( socket, net::buffer(write_queue.front().msg)
                         , [this](auto ec, auto n)
                           { on_write(ec, n); });
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


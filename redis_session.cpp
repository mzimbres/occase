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
#include "async_read_resp.hpp"

namespace
{

inline
void fail_tmp(boost::system::error_code ec, char const* what)
{
   std::cerr << what << ": " << ec.message() << "\n";
}

}

namespace aedis
{

void redis_session::on_resolve( boost::system::error_code ec
                              , tcp::resolver::results_type results)
{
   if (ec)
      return fail_tmp(ec, "resolve");

   auto const handler = [this](auto ec, auto Iterator)
   {
      on_connect(ec, Iterator);
   };

   asio::async_connect(socket, results, handler);
}

void redis_session::run()
{
   auto handler = [this](auto ec, auto res)
   {
      on_resolve(ec, res);
   };

   resolver.async_resolve(cf.host, cf.port, handler);
}

void redis_session::send(redis_req req)
{
   auto const is_empty = std::empty(write_queue);
   write_queue.push(std::move(req));

   if (is_empty && socket.is_open())
      asio::async_write( socket, asio::buffer(write_queue.front().msg)
                       , [this](auto ec, auto n)
                         {on_write(ec, n);});
}

void redis_session::close()
{
   boost::system::error_code ec;
   socket.shutdown(tcp::socket::shutdown_send, ec);
   if (ec)
      fail_tmp(ec, "close");

   socket.close(ec);
   if (ec)
      fail_tmp(ec, "close");
}

void redis_session::start_reading_resp()
{
   auto const handler = [this]( boost::system::error_code ec
                              , std::vector<std::string> res)
   {
      on_resp(ec, std::move(res));
   };

   data.clear();
   async_read_resp(socket, &data, handler);
}

void redis_session::on_connect( boost::system::error_code ec
                              , asio::ip::tcp::endpoint const& endpoint)
{
   if (ec) {
      fail_tmp(ec, "on_connect");
      return;
   }

   asio::ip::tcp::no_delay option(true);
   socket.set_option(option);

   start_reading_resp();

   // Consumes any messages that have been eventually posted while the
   // connection was not established.
   if (!std::empty(write_queue))
      asio::async_write( socket, asio::buffer(write_queue.front().msg)
                       , [this](auto ec, auto n)
                         { on_write(ec, n); });
}

void redis_session::on_resp( boost::system::error_code ec
                           , std::vector<std::string> res)
{
   auto cmd = redis_cmd::unsolicited;
   if (!std::empty(write_queue))
      cmd = write_queue.front().cmd;

   on_msg_handler(ec, std::move(res), cmd);

   if (!ec && socket.is_open()) {
      start_reading_resp();
      if (!std::empty(write_queue))
         write_queue.pop();
      if (!std::empty(write_queue))
         asio::async_write( socket, asio::buffer(write_queue.front().msg)
                          , [this](auto ec, auto n)
                            { on_write(ec, n); });
   }
}

void redis_session::on_write( boost::system::error_code ec
                            , std::size_t n)
{
   if (ec) {
      fail_tmp(ec, "on_write");
      return;
   }
}

}


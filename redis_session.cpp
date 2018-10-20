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

void redis_session::send(interaction i)
{
   auto const is_empty = std::empty(write_queue);
   write_queue.push(std::move(i));

   if (is_empty && socket.is_open()) {
      auto const handler = [this](auto ec, auto n)
      { on_write(ec, n); };

      //std::cout << "is_open: " << socket.is_open() << std::endl;
      asio::async_write( socket, asio::buffer(write_queue.front().cmd)
                       , handler);
   }
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

void redis_session::on_resp_chunk( boost::system::error_code ec
                                 , std::size_t n
                                 , int counter
                                 , bool bulky_str_read)
{
   if (ec) {
      fail_tmp(ec, "on_resp_chunk");
      return;
   }

   if (std::size(data) < 4) {
      std::cout << "on_resp_chunk: Invalid redis response. Aborting ..."
                << std::endl;
      return;
   }

   if (!bulky_str_read && counter != 0) {
      auto const c = data.front();
      switch (c) {
         case '$':
         {
            asio::async_read_until( socket
                                  , asio::dynamic_buffer(data)
                                  , delim
                                  , [this, counter](auto ec, auto n)
                                    { on_resp_chunk( ec, n, counter - 1
                                                   , true); });
            return;
         }
         break;
         case '*':
         {
            assert(counter == 1);
            // counter = ...
         }
         break;
         case '+':
         case '-':
         case ':':
         break;
         default:
            assert(false);
      }
   }

   if (counter == 0) {
      // We are done.
      asio::post(socket.get_io_context(), [this]() { on_resp(); });
      return;
   }

   asio::async_read_until( socket, asio::dynamic_buffer(data), delim
                         , [this, counter](auto ec, auto n)
                           { on_resp_chunk(ec, n, counter - 1, false); });
}

void redis_session::start_reading_resp()
{
   //auto const handler = [this]( boost::system::error_code ec
   //                           , std::size_t n)
   //{ on_resp(ec, n); };

   auto const handler = [this](auto ec, std::size_t n)
   {
      on_resp_chunk(ec, n, 1, false);
   };

   asio::async_read_until( socket, asio::dynamic_buffer(data)
                         , delim, handler);
}

void redis_session::on_connect( boost::system::error_code ec
                              , asio::ip::tcp::endpoint const& endpoint)
{
   if (ec) {
      std::cout << "on_connect: " << ec.message() << std::endl;
      return;
   }

   asio::ip::tcp::no_delay option(true);
   socket.set_option(option);

   start_reading_resp();

   if (std::empty(write_queue))
      return;

   // Consumes any messages that have been eventually posted while the
   // connection was not established.
   auto const handler = [this](auto ec, auto n)
   {
      on_write(ec, n);
   };

   asio::async_write( socket, asio::buffer(write_queue.front().cmd)
                    , handler);
}

void redis_session::on_resp()
{
   write_queue.front().action({}, std::move(data));

   start_reading_resp();
   write_queue.pop();
   waiting_response = false;

   if (std::empty(write_queue))
      return;

   auto const handler = [this](auto ec, auto n)
   { on_write(ec, n); };

   //std::cout << "on_write: Writing more." << std::endl;
   asio::async_write( socket, asio::buffer(write_queue.front().cmd)
                      , handler);
}

void redis_session::on_write( boost::system::error_code ec
                            , std::size_t n)
{
   if (ec) {
     std::cout << "on_write error: " << ec.message() << std::endl;
     close();
     return;
   }

   //std::cout << "on_write popping ===> " << write_queue.front().cmd
   //          << " " << n << std::endl;
   waiting_response = true;
}

}


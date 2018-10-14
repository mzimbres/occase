#include <deque>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string_view>

#include <boost/asio.hpp>
#include <boost/asio/bind_executor.hpp>
#include <boost/asio/ip/tcp.hpp>

#include "redis_session.hpp"

using boost::asio::ip::tcp;

namespace aedis
{

void redis_session::run()
{
   auto const handler = [this](auto ec, auto Iterator)
   {
      on_connect(ec);
   };

   boost::asio::async_connect(rs.next_layer(), endpoints, handler);
}

void redis_session::send(interaction i)
{
   boost::asio::post( rs.next_layer().get_io_context()
                    , [this, ii = std::move(i)]() { do_write(std::move(ii));}
                    );
}

void redis_session::close()
{
   boost::asio::post( rs.next_layer().get_io_context()
                    , [this]() { do_close(); });
}

void redis_session::do_read()
{
   auto const handler = [this]( boost::system::error_code ec
                              , std::size_t n)
   { on_read(ec, n); };

   rs.async_read(buffer, handler);
}

void redis_session::on_connect(boost::system::error_code ec)
{
   if (ec) {
      std::cout << "on_connect: " << ec.message() << std::endl;
      return;
   }

   boost::asio::ip::tcp::no_delay option(true);
   rs.next_layer().set_option(option);

   do_read();
}

void redis_session::on_read(boost::system::error_code ec, std::size_t n)
{
   if (ec) {
      if (ec == boost::asio::error::operation_aborted) {
         // Abortion can be caused by a socket shutting down and closing.
         // We have no cleanup to perform.
         return;
      }

      std::cout << "Error" << std::endl;
      return;
   }

   assert(n == std::size(result));

   write_queue.front().action(ec, result);
   buffer.consume(std::size(buffer));

   do_read();
   write_queue.pop();
   waiting_response = false;

   if (std::empty(write_queue))
      return;

   auto const handler = [this](auto ec, auto n)
   { on_write(ec, n); };

   //std::cout << "on_write: Writing more." << std::endl;
   boost::asio::async_write( rs
                           , boost::asio::buffer(write_queue.front().cmd)
                           , handler);
}

void redis_session::do_write(interaction i)
{
   auto const is_empty = std::empty(write_queue);
   write_queue.push(std::move(i));

   if (is_empty) {
      auto const handler = [this](auto ec, auto n)
      { on_write(ec, n); };

      //std::cout << "async_write ===> " << write_queue.front().cmd 
      //          << std::endl;
      boost::asio::async_write( rs
                              , boost::asio::buffer(write_queue.front().cmd)
                              , handler);
   }
}

void redis_session::on_write( boost::system::error_code ec
                            , std::size_t n)
{
   if (ec) {
     std::cout << "on_write error: " << ec.message() << std::endl;
     do_close();
     return;
   }

   //std::cout << "on_write popping ===> " << write_queue.front().cmd
   //          << " " << n << std::endl;
   waiting_response = true;
}

void redis_session::do_close()
{
   //std::cout << "do_close." << std::endl;
   rs.next_layer().close();
}

}


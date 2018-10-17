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

   boost::asio::async_connect(socket, endpoints, handler);
}

void redis_session::send(interaction i)
{
   boost::asio::post( socket.get_io_context()
                    , [this, ii = std::move(i)]() { do_write(std::move(ii));}
                    );
}

void redis_session::close()
{
   boost::asio::post( socket.get_io_context() , [this]() { do_close(); });
}

void redis_session::on_resp_chunk( boost::system::error_code ec
                                 , std::size_t n)
{
   if (ec) {
      std::cout << "on_resp_chunk: " << ec.message() << std::endl;
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
         case '+':
         case '-':
         case ':':
         {
            --counter;
         }
         break;
         case '$':
         {
            --counter;
            bulky_str_read = true;
            boost::asio::async_read_until( socket
                                         , boost::asio::dynamic_buffer(data)
                                         , delim
                                         , [this](auto ec, auto n)
                                           { on_resp_chunk(ec, n); });
            return;
         }
         break;
         case '*':
         {
            assert(counter == 1);
            // counter = ...
         }
         break;
         default:
         {
            std::cout << "on_resp_chunk: Invalid redis response. Aborting ..."
                      << std::endl;
            return;
         }
      }
   }

   bulky_str_read = false;

   if (counter == 0) {
      // We are done.
      boost::asio::post( socket.get_io_context()
                       , [this]() { on_resp(); });
      counter = 1;
   } else {
      boost::asio::async_read_until( socket
            , boost::asio::dynamic_buffer(data)
                                   , delim,
            [this](auto ec, auto n) { on_resp_chunk(ec, n); });
   }
   return;
}

void redis_session::do_read_resp()
{
   //auto const handler = [this]( boost::system::error_code ec
   //                           , std::size_t n)
   //{ on_resp(ec, n); };

   auto const handler = [this](auto ec, std::size_t n)
   { on_resp_chunk(ec, n); };

   boost::asio::async_read_until( socket
            , boost::asio::dynamic_buffer(data)
            , delim, handler);
}

void redis_session::on_connect(boost::system::error_code ec)
{
   if (ec) {
      std::cout << "on_connect: " << ec.message() << std::endl;
      return;
   }

   boost::asio::ip::tcp::no_delay option(true);
   socket.set_option(option);

   do_read_resp();
}

void redis_session::on_resp()
{
   write_queue.front().action({}, std::move(data));

   do_read_resp();
   write_queue.pop();
   waiting_response = false;

   if (std::empty(write_queue))
      return;

   auto const handler = [this](auto ec, auto n)
   { on_write(ec, n); };

   //std::cout << "on_write: Writing more." << std::endl;
   boost::asio::async_write( socket
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
      boost::asio::async_write( socket
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
   socket.close();
}

}


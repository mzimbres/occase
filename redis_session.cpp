#include <deque>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string_view>

#include <boost/asio.hpp>
#include <boost/asio/bind_executor.hpp>
#include <boost/asio/ip/tcp.hpp>

#include "resp.hpp"
#include "redis_session.hpp"

using boost::asio::ip::tcp;

namespace aedis
{

void redis_session::run()
{
   auto const handler = [p = shared_from_this()](auto ec, auto Iterator)
   {
      p->on_connect(ec);
   };

   boost::asio::async_connect(rs.next_layer(), endpoints, handler);
}

void redis_session::write(std::string msg)
{
   msg += "\r\n";
   auto const handler = [p = shared_from_this(), m = std::move(msg)]()
   {
      p->do_write(std::move(m));
   };

   boost::asio::post(rs.next_layer().get_io_context(), handler);
}

void redis_session::close()
{
   auto const handler = [p = shared_from_this()]()
   {
      p->do_close();
   };

   boost::asio::post(rs.next_layer().get_io_context(), handler);
}

void redis_session::do_read()
{
   auto const handler =
      [p = shared_from_this()](boost::system::error_code ec, std::size_t n)
   {
      p->on_read(ec, n);
   };

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
      std::cout << "Error" << std::endl;
      return;
   }

   resp_response resp(result);
   resp.process_response();
   buffer.consume(std::size(buffer));
   do_read();
}

void redis_session::do_write(std::string msg)
{
   auto const is_empty = std::empty(write_queue);
   write_queue.push(std::move(msg));

   if (is_empty) {
      auto const handler = [p = shared_from_this()](auto ec, auto n)
      {
         p->on_write(ec, n);
      };

      //std::cout << "async_write ===> " << write_queue.front() << std::endl;
      boost::asio::async_write( rs
                              , boost::asio::buffer(write_queue.front())
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

   //std::cout << "on_write popping ===> " << write_queue.front()
   //          << " " << n << std::endl;
   write_queue.pop();

   if (std::empty(write_queue))
      return;

   auto const handler = [p = shared_from_this()](auto ec, auto n)
   {
      p->on_write(ec, n);
   };

   //std::cout << "on_write: Writing more." << std::endl;
   boost::asio::async_write( rs
                           , boost::asio::buffer(write_queue.front())
                           , handler);
}

void redis_session::do_close()
{
   std::cout << "do_close." << std::endl;
   rs.next_layer().close();
}

}


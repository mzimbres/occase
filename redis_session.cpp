#include <deque>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string_view>

#include <boost/asio.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/bind_executor.hpp>
#include <boost/asio/ip/tcp.hpp>

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

   boost::asio::async_connect(socket, endpoints
                          , boost::asio::bind_executor(strand, handler));
}

void redis_session::write(std::string msg)
{
   msg += "\r\n";
   auto const handler = [p = shared_from_this(), m = std::move(msg)]()
   {
      p->do_write(std::move(m));
   };

   boost::asio::post(boost::asio::bind_executor(strand, handler));
}

void redis_session::close()
{
   auto const handler = [p = shared_from_this()]()
   {
      p->do_close();
   };

   boost::asio::post(boost::asio::bind_executor(strand, handler));
}

void redis_session::on_connect(boost::system::error_code ec)
{
   if (ec) {
      std::cout << "on_connect: " << ec.message() << std::endl;
      return;
   }

   boost::asio::ip::tcp::no_delay option(true);
   socket.set_option(option);

   do_read({}, 0);
}

void redis_session::do_read(boost::system::error_code ec, std::size_t n)
{
   if (ec) {
      std::cout << "do_read closing." << std::endl;
      do_close();
      return;
   }

   std::copy( std::begin(message), std::begin(message) + n
            , std::back_inserter(result));

   if (n < msg_size) {
      std::string_view v {result.data(), std::size(result)};
      std::cout << v << "\n";
      result.resize(0);
   }

   //std::cout << "Vector size " << std::size(message)
   //          << " -- " << n << std::endl;
   message.resize(msg_size);
   //boost::asio::async_read( socket
   //                       , boost::asio::buffer(message)
   //                       , boost::asio::transfer_all()
   //                       , boost::asio::bind_executor(strand, handler));
   //boost::asio::async_read_until( socket
   //                       , boost::asio::dynamic_buffer(message)
   //                       , "\r\n"
   //                       , boost::asio::bind_executor(strand, handler));

   auto const handler = [p = shared_from_this()](auto ec, auto n)
   {
      p->do_read(ec, n);
   };

   socket.async_read_some( boost::asio::buffer(message)
                         , boost::asio::bind_executor(strand, handler));
}

void redis_session::do_write(std::string msg)
{
   auto const is_empty = write_queue.empty();
   write_queue.push_back(std::move(msg));

   if (is_empty) {
      auto const handler = [p = shared_from_this()](auto ec, auto n)
      {
         p->on_write(ec, n);
      };

      //std::cout << "async_write ===> " << write_queue.front() << std::endl;
      boost::asio::async_write( socket
                           , boost::asio::buffer(write_queue.front())
                           , boost::asio::bind_executor(strand, handler));
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
   write_queue.pop_front();

   if (write_queue.empty())
      return;

   auto const handler = [p = shared_from_this()](auto ec, auto n)
   {
      p->on_write(ec, n);
   };

   //std::cout << "on_write: Writing more." << std::endl;
   boost::asio::async_write( socket
                        , boost::asio::buffer(write_queue.front())
                        , boost::asio::bind_executor(strand, handler));
}

void redis_session::do_close()
{
   std::cout << "do_close." << std::endl;
   socket.close();
}

}


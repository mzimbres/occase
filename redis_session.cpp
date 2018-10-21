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

   if (is_empty && socket.is_open())
      asio::async_write( socket, asio::buffer(write_queue.front().cmd)
                       , [this](auto ec, auto n) {on_write(ec, n);});
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
                                 , bool bulky_str_read
                                 , std::size_t pos)
{
   if (ec) {
      fail_tmp(ec, "on_resp_chunk");
      return;
   }

   if (n < 4) {
      std::cout << "on_resp_chunk: Invalid redis response. Aborting ..."
                << std::endl;
      return;
   }

   auto foo = false;
   if (!bulky_str_read && counter != 0) {
      switch (data[pos]) {
         case '$': foo = true; break;
         case '+': break;
         case '-': break;
         case ':': break;
         default: // '*'
         {
            assert(counter == 1);
            auto p = std::cbegin(data);
            counter = aedis::get_length(++p);
         }
      }
   }

   if (counter == 0) {
      asio::post(socket.get_io_context(), [this]() { on_resp({}); });
      return;
   }

   auto const rs = pos + n;
   asio::async_read_until( socket, asio::dynamic_buffer(data), delim
                         , [this, counter, rs, foo](auto ec, auto n2)
                           { on_resp_chunk( ec, n2, counter - 1
                                          , foo, rs); });
}

void redis_session::start_reading_resp()
{
   asio::async_read_until( socket, asio::dynamic_buffer(data)
                         , delim
                         , [this](auto ec, std::size_t n)
                           { on_resp_chunk(ec, n, 1, false, 0); });
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

   if (std::empty(write_queue))
      return;

   // Consumes any messages that have been eventually posted while the
   // connection was not established.
   asio::async_write( socket, asio::buffer(write_queue.front().cmd)
                    , [this](auto ec, auto n) { on_write(ec, n); });
}

void redis_session::on_resp(boost::system::error_code ec)
{
   if (std::empty(write_queue)) {
      std::cout << data << std::endl;
      data.clear();
      start_reading_resp();
      return;
   }

   auto data_tmp = std::move(data);
   start_reading_resp();
   write_queue.front().action(ec, data_tmp);
   write_queue.pop();

   if (std::empty(write_queue))
      return;

   asio::async_write( socket, asio::buffer(write_queue.front().cmd)
                    , [this](auto ec, auto n) { on_write(ec, n); });
}

void redis_session::on_write( boost::system::error_code ec
                            , std::size_t n)
{
   if (ec) {
      fail_tmp(ec, "on_write");
      close();
      return;
   }
}

}


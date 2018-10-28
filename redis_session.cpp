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

void redis_session::send(std::string msg)
{
   auto const is_empty = std::empty(write_queue);
   write_queue.push(std::move(msg));

   if (is_empty && socket.is_open())
      asio::async_write( socket, asio::buffer(write_queue.front())
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
                                 , std::size_t n, int counter
                                 , bool bulky_str_read)
{
   if (ec) {
      fail_tmp(ec, "on_resp_chunck");
      asio::post(socket.get_io_context(), [this, ec]() { on_resp(ec); });
      return;
   }

   if (n < 4) {
      std::cout << "on_resp_chunk: Invalid redis response. Aborting ..."
                << std::endl;
      // TODO: Call on_resp here with error.
      return;
   }

   auto foo = false;
   if (bulky_str_read) {
      res.push_back(data.substr(0, n - 2));
      --counter;
   } else {
      if (counter != 0) {
         switch (data.front()) {
            case '$':
            {
               // TODO: Do not push in the vector but find a way to
               // report nil.
               if (data.compare(1, 2, "-1") == 0) {
                  res.push_back({});
                  --counter;
               } else {
                  foo = true;
               }
            }
            break;
            case '+':
            case '-':
            case ':':
            {
               res.push_back(data.substr(1, n - 3));
               --counter;
            }
            break;
            case '*':
            {
               assert(counter == 1);
               counter = get_length(data.data() + 1);
            }
            break;
            default:
               assert(false);
         }
      }
   }

   data.erase(0, n);

   if (counter == 0) {
      asio::post(socket.get_io_context(), [this]() { on_resp({}); });
      return;
   }

   asio::async_read_until( socket, asio::dynamic_buffer(data), delim
                         , [this, counter, foo](auto ec, auto n2)
                           { on_resp_chunk(ec, n2, counter, foo); });
}

void redis_session::start_reading_resp()
{
   asio::async_read_until( socket, asio::dynamic_buffer(data)
                         , delim
                         , [this](auto ec, std::size_t n)
                           { on_resp_chunk(ec, n, 1, false); });
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
      asio::async_write( socket, asio::buffer(write_queue.front())
                       , [this](auto ec, auto n) { on_write(ec, n); });
}

void redis_session::on_resp(boost::system::error_code ec)
{
   msg_handler(ec, std::move(res));

   if (!ec && socket.is_open()) {
      start_reading_resp();
      if (!std::empty(write_queue))
         write_queue.pop();
      if (!std::empty(write_queue))
         asio::async_write( socket, asio::buffer(write_queue.front())
                          , [this](auto ec, auto n) { on_write(ec, n); });
   }
}

void redis_session::on_write( boost::system::error_code ec
                            , std::size_t n)
{
   if (ec) {
      fail_tmp(ec, "on_write");
      //close();
      return;
   }
}

}


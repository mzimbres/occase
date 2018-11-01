#pragma once

#include <array>
#include <queue>
#include <vector>
#include <string>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <functional>
#include <type_traits>

#include <boost/asio.hpp>
#include <boost/asio/bind_executor.hpp>
#include <boost/asio/ip/tcp.hpp>

#include "resp.hpp"
#include "config.hpp"

// TODO: Improve this according to
// boost_1_67_0/boost/asio/impl/read.hpp : 654 and 502

template <typename ReadHandler>
class read_resp_op {
private:
   static std::string_view constexpr delim {"\r\n"};
   boost::asio::ip::tcp::socket& stream;
   ReadHandler handler;
   std::string* data;
   std::vector<std::string> res;
   int counter;
   bool bulky_str_read;

public:
   read_resp_op( boost::asio::ip::tcp::socket& stream_
               , std::string* data_
               , ReadHandler handler_)
   : stream(stream_)
   , data(data_)
   , handler(std::move(handler_))
   { }

   read_resp_op(read_resp_op const& other)
   : stream(other.stream)
   , handler(other.handler)
   , res(other.res)
   , data(other.data)
   , counter(other.counter)
   , bulky_str_read(other.bulky_str_read)
   { }

   read_resp_op(read_resp_op&& other)
   : stream(other.stream)
   , handler(std::move(other.handler))
   , res(std::move(other.res))
   , data(other.data)
   , counter(other.counter)
   , bulky_str_read(other.bulky_str_read)
   { }

   void operator()( boost::system::error_code ec, std::size_t n
                  , bool start = false)
   {
      if (start) {
         counter = 1;
         bulky_str_read = false;
         asio::async_read_until( stream, asio::dynamic_buffer(*data)
                               , delim, std::move(*this));
         return;
      }

      if (ec) {
         handler(ec, {});
         return;
      }

      if (n < 4) {
         std::cout << "on_resp_chunk: Invalid redis response. Aborting ..."
                   << std::endl;
         // TODO: Change handler interface to be able to report error.
         handler(ec, {});
         return;
      }

      auto foo = false;
      if (bulky_str_read) {
         res.push_back(data->substr(0, n - 2));
         --counter;
      } else {
         if (counter != 0) {
            switch (data->front()) {
               case '$':
               {
                  // TODO: Do not push in the vector but find a way to
                  // report nil.
                  if (data->compare(1, 2, "-1") == 0) {
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
                  res.push_back(data->substr(1, n - 3));
                  --counter;
               }
               break;
               case '*':
               {
                  assert(counter == 1);
                  counter = get_length(data->data() + 1);
               }
               break;
               default:
                  assert(false);
            }
         }
      }

      data->erase(0, n);

      if (counter == 0) {
         handler({}, std::move(res));
         return;
      }

      bulky_str_read = foo;
      asio::async_read_until( stream, asio::dynamic_buffer(*data), delim
                            , std::move(*this));
   }
};

template <typename ReadHandler>
void async_read_resp( boost::asio::ip::tcp::socket& s
                    , std::string* data, ReadHandler handler)
{
  read_resp_op<ReadHandler>(s, data, std::move(handler))({}, 0, true);
}

namespace aedis
{

struct redis_session_cf {
   std::string host;
   std::string port;
};

using redis_handler_type =
   std::function<void(boost::system::error_code, std::vector<std::string>)>;

class redis_session {
private:
   static std::string_view constexpr delim {"\r\n"};

   redis_session_cf cf;
   tcp::resolver resolver;
   tcp::socket socket;
   std::string data;
   std::queue<std::string> write_queue;
   redis_handler_type msg_handler = [](auto, auto){};

   void start_reading_resp();

   void on_resolve( boost::system::error_code ec
                  , tcp::resolver::results_type results);
   void on_connect( boost::system::error_code ec
                  , asio::ip::tcp::endpoint const& endpoint);
   void on_resp( boost::system::error_code ec
               , std::vector<std::string> res);
   void on_write(boost::system::error_code ec, std::size_t n);

public:
   redis_session(redis_session_cf cf_, asio::io_context& ioc)
   : cf(cf_)
   , resolver(ioc) 
   , socket(ioc)
   { }

   void run();
   void send(std::string msg);
   void close();
   void set_msg_handler(redis_handler_type handler)
   { msg_handler = std::move(handler);};
};

}


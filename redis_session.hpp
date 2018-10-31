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

#include "config.hpp"

namespace detail
{

template < typename AsyncReadStream
         , typename DynamicBuffer
         , typename ReadHandler>
class read_resp_op {
// private:
public:
   AsyncReadStream& stream_;
   DynamicBuffer buffers_;
   int start_;
   std::size_t total_transferred_;
   ReadHandler handler_;
public:
   template <typename BufferSequence>
   read_resp_op( AsyncReadStream& stream
               , BufferSequence buffers
               , ReadHandler handler)
   : stream_(stream)
   , buffers_(std::move(buffers))
   , start_(0)
   , total_transferred_(0)
   , handler_(std::move(handler))
   { }

   read_resp_op(read_resp_op const& other)
   : stream_(other.stream_)
   , buffers_(other.buffers_)
   , start_(other.start_)
   , total_transferred_(other.total_transferred_)
   , handler_(other.handler_)
   { }

   read_resp_op(read_resp_op&& other)
   : stream_(other.stream_)
   , buffers_(BOOST_ASIO_MOVE_CAST(DynamicBuffer)(other.buffers_))
   , start_(other.start_)
   , total_transferred_(other.total_transferred_)
   , handler_(BOOST_ASIO_MOVE_CAST(ReadHandler)(other.handler_))
   { }

   void operator()( boost::system::error_code const& ec
                  , std::size_t bytes_transferred
                  , int start = 0)
   {
      stream_.async_read_some( buffers_.prepare(0)
                                , std::move(*this));
      handler_(ec, static_cast<const std::size_t&>(total_transferred_));
   }
};

}

template < typename AsyncReadStream
         , typename DynamicBuffer
         , typename ReadHandler>
inline void
async_read_resp( AsyncReadStream& s
               , DynamicBuffer&& buffers
               , ReadHandler handler)
{
  detail::read_resp_op< AsyncReadStream
                        , typename std::decay<DynamicBuffer>::type
                        , ReadHandler
                        >( s
                         , std::move(buffers)
                         , std::move(handler)
                         )(boost::system::error_code(), 0, 1);
}

namespace aedis
{

// TODO: Use a strategy similar to 
// boost_1_67_0/boost/asio/impl/read.hpp : 654 and 502

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
   std::vector<std::string> res;
   std::queue<std::string> write_queue;
   redis_handler_type msg_handler = [](auto, auto){};

   void start_reading_resp();

   void on_resolve( boost::system::error_code ec
                  , tcp::resolver::results_type results);
   void on_connect( boost::system::error_code ec
                  , asio::ip::tcp::endpoint const& endpoint);
   void on_resp(boost::system::error_code ec);
   void on_resp_chunk( boost::system::error_code ec
                     , std::size_t n, int counter
                     , bool bulky_str_read);
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


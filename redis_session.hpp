#pragma once

#include <array>
#include <queue>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>

#include <boost/asio.hpp>
#include <boost/asio/bind_executor.hpp>
#include <boost/asio/ip/tcp.hpp>

namespace aedis
{

class stream {
public:
   using next_layer_type = boost::asio::ip::tcp::socket;

private:
   next_layer_type socket;
   std::array<char, 3> message;
   std::vector<char> result;

   template<class ReadHandler>
   void do_read_some( std::vector<char>* buffer
                    , ReadHandler&& handler)
   {
      auto handler2 = [this, buffer, handler](auto ec, auto n)
      {
         on_read_some(ec, n, buffer, handler);
      };

      socket.async_read_some(boost::asio::buffer(message), handler2);
   }

   template<class ReadHandler>
   void on_read_some( boost::system::error_code ec
                    , std::size_t n
                    , std::vector<char>* buffer
                    , ReadHandler&& handler)
   {
      if (ec) {
         handler(ec, n);
         return;
      }

      std::copy( std::begin(message), std::begin(message) + n
               , std::back_inserter(result));

      if (n < std::size(message)) {
         *buffer = std::move(result);
         //boost::asio::buffer_copy(result, buffer);
         auto const hh = [handler, n = std::size(result)]()
         {
            handler({}, n);
         };
         boost::asio::post(socket.get_io_context(), hh);
         return;
      }
      
      do_read_some(buffer, handler);
   }

public:
   stream(boost::asio::io_context& ioc)
   : socket(ioc)
   {}

   next_layer_type& next_layer() {return socket;}
   next_layer_type const& next_layer() const {return socket;}

   template < class ConstBufferSequence
            , class WriteHandler>
   auto async_write( ConstBufferSequence const& buffers
                   , WriteHandler&& handler)
   {
      boost::asio::async_write(socket, buffers, handler);
   }

   template< class ConstBufferSequence
           , class WriteHandler>
   auto async_write_some( ConstBufferSequence const& buffers
                        , WriteHandler&& handler)
   {
      socket.async_write_some(buffers, handler);
   }

   template<class ReadHandler>
   auto async_read( std::vector<char>* buffer
                  , ReadHandler&& handler)
   {
      do_read_some(buffer, handler);
   }
};

class redis_session :
  public std::enable_shared_from_this<redis_session> {
private:
   stream rs;
   std::vector<char> result;
   std::queue<std::string> write_queue;
   boost::asio::ip::tcp::resolver::results_type endpoints;

   void on_connect(boost::system::error_code ec);
   void on_read(boost::system::error_code ec, std::size_t n);
   void do_write(std::string msg);
   void do_read();
   void on_write(boost::system::error_code ec, std::size_t n);
   void do_close();

public:
   redis_session( boost::asio::io_context& ioc_
                , boost::asio::ip::tcp::resolver::results_type endpoints_)
   : rs(ioc_)
   , endpoints(endpoints_)
   { }

   void run();
   void write(std::string msg);
   void close();
};

}


#pragma once

#include <functional>

#include <boost/asio.hpp>

namespace aedis
{

template < class NextLayer
         , int N>
class stream {
public:
   using next_layer_type = NextLayer;

private:
   next_layer_type socket;

   template< class DynamicBuffer
           , class ReadHandler>
   void do_read_some( std::reference_wrapper<DynamicBuffer> buffer
                    , ReadHandler&& handler)
   {
      auto handler2 = [this, buffer, handler](auto ec, auto n)
      {
         on_read_some(ec, n, buffer, handler);
      };

      socket.async_read_some( boost::asio::buffer(buffer.get().prepare(N))
                            , handler2);
   }

   template< class DynamicBuffer
           , class ReadHandler>
   void on_read_some( boost::system::error_code ec
                    , std::size_t n
                    , std::reference_wrapper<DynamicBuffer> buffer
                    , ReadHandler&& handler)
   {
      if (ec) {
         handler(ec, n);
         return;
      }

      buffer.get().commit(n);

      if (n < N) {
         handler({}, n);
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

   template< class DynamicBuffer
           , class ReadHandler>
   auto async_read( DynamicBuffer& buffer
                  , ReadHandler&& handler)
   {
      do_read_some(std::ref(buffer), handler);
   }
};

}


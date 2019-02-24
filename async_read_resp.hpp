#pragma once

#include <string>
#include <vector>
#include <numeric>

#include <boost/asio.hpp>
#include <boost/asio/async_result.hpp>
#include <boost/beast/websocket.hpp>

namespace net = boost::asio;
namespace beast = boost::beast;

// TODO: Improve this according to
// boost_1_67_0/boost/asio/impl/read.hpp : 654 and 502
//

namespace rt::redis
{

// Converts a decimal number in ascii format to integer.
inline
std::size_t get_length(char const* p)
{
   std::size_t len = 0;
   while (*p != '\r') {
       len = (10 * len) + (*p - '0');
       p++;
   }
   return len;
}

struct resp_buffer {
   std::string data;
   std::vector<std::string> res;
};

inline
std::string get_bulky_str(std::string param)
{
   auto const s = std::size(param);
   return "$"
        + std::to_string(s)
        + "\r\n"
        + std::move(param)
        + "\r\n";
}

template <class Iter>
auto resp_assemble(char const* c, Iter begin, Iter end)
{
   auto const d = std::distance(begin, end);
   std::string payload = "*";
   payload += std::to_string(d + 1);
   payload += "\r\n";
   payload += get_bulky_str(c);

   auto const op = [](auto a, auto b)
   { return std::move(a) + get_bulky_str(std::move(b)); };

   return std::accumulate(begin , end, std::move(payload), op);
}

template < class AsyncStream
         , class Handler>
struct read_resp_op {
   AsyncStream& stream;
   Handler handler;
   resp_buffer* buffer = nullptr;
   int start = 0;
   int counter = 1;
   bool bulky_str_read = false;

   using allocator_type =
      net::associated_allocator_t<Handler>;

   using executor_type =
      net::associated_executor_t<
         Handler, decltype(std::declval<AsyncStream&>().get_executor())>;

   read_resp_op( AsyncStream& stream_
               , resp_buffer* buffer_
               , Handler handler_)
   : stream(stream_)
   , handler(std::move(handler_))
   , buffer(buffer_)
   { }

    allocator_type get_allocator() const noexcept
    { return net::get_associated_allocator(handler); }


    executor_type get_executor() const noexcept
    {
        return net::get_associated_executor( handler
                                           , stream.get_executor());
    }

   void operator()( boost::system::error_code const& ec, std::size_t n
                  , int start_ = 0)
   {
      switch (start = start_) {
         for (;;) {
            case 1:
            net::async_read_until( stream, net::dynamic_buffer(buffer->data)
                                 , "\r\n", std::move(*this));
            return; default:

            if (ec || n < 3) {
               handler(ec);
               return;
            }

            auto str_flag = false;
            if (bulky_str_read) {
               buffer->res.push_back(buffer->data.substr(0, n - 2));
               --counter;
            } else {
               if (counter != 0) {
                  switch (buffer->data.front()) {
                     case '$':
                     {
                        // TODO: Do not push in the vector but find a way to
                        // report nil.
                        if (buffer->data.compare(1, 2, "-1") == 0) {
                           buffer->res.push_back({});
                           --counter;
                        } else {
                           str_flag = true;
                        }
                     }
                     break;
                     case '+':
                     case '-':
                     case ':':
                     {
                        buffer->res.push_back(buffer->data.substr(1, n - 3));
                        --counter;
                     }
                     break;
                     case '*':
                     {
                        assert(counter == 1);
                        counter = get_length(buffer->data.data() + 1);
                     }
                     break;
                     default:
                        assert(false);
                  }
               }
            }

            buffer->data.erase(0, n);

            if (counter == 0) {
               handler(boost::system::error_code{});
               return;
            }

            bulky_str_read = str_flag;
         }
      }
   }
};

template < class AsyncReadStream
         , class ReadHandler
         >
inline void*
asio_handler_allocate( std::size_t size
                     , read_resp_op< AsyncReadStream
                                   , ReadHandler
                                   >* this_handler)
{
   return boost_asio_handler_alloc_helpers::allocate(
      size, this_handler->handler);
}

template < class AsyncReadStream
         , class ReadHandler
         >
inline void
asio_handler_deallocate( void* pointer
                       , std::size_t size
                       , read_resp_op< AsyncReadStream
                                     , ReadHandler
                                     >* this_handler)
{
   boost_asio_handler_alloc_helpers::deallocate(
         pointer, size, this_handler->handler);
}

template < class AsyncReadStream
         , class ReadHandler
         >
inline bool
asio_handler_is_continuation( read_resp_op< AsyncReadStream
                                          , ReadHandler
                                          >* this_handler)
{
   return this_handler->start == 0 ? true
      : boost_asio_handler_cont_helpers::is_continuation(
            this_handler->handler);
}

template < class Function
         , class AsyncReadStream
         , class ReadHandler
         >
inline void
asio_handler_invoke( Function& function
                   , read_resp_op< AsyncReadStream
                                 , ReadHandler
                                 >* this_handler)
{
   boost_asio_handler_invoke_helpers::invoke(
         function, this_handler->handler);
}

template < class Function
         , class AsyncReadStream
         , class ReadHandler
         >
inline void
asio_handler_invoke( Function const& function
                   , read_resp_op< AsyncReadStream
                                 , ReadHandler
                                 >* this_handler)
{
   boost_asio_handler_invoke_helpers::invoke(
         function, this_handler->handler_);
}

template < class AsyncStream
         , class CompletionToken>
BOOST_ASIO_INITFN_RESULT_TYPE(CompletionToken, void(boost::beast::error_code))
async_read_resp( AsyncStream& s
               , resp_buffer* buffer
               , CompletionToken&& handler)
{
   // TODO: Write a similar macro to check the handler signature.
   //BOOST_ASIO_READ_HANDLER_CHECK(CompletionToken, handler) type_check;

   using read_handler_signature = void (boost::system::error_code const&);

   net::async_completion< CompletionToken
                        , read_handler_signature
                        > init {handler};

   using handler_type = 
      read_resp_op< AsyncStream
                  , BOOST_ASIO_HANDLER_TYPE( CompletionToken
                                           , read_handler_signature)
                  >;

   handler_type {s, buffer, init.completion_handler}({}, 0, 1);

   return init.result.get();
}

}


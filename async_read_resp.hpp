#pragma once

#include <boost/asio.hpp>
#include <boost/asio/async_result.hpp>

#include "resp.hpp"

// TODO: Improve this according to
// boost_1_67_0/boost/asio/impl/read.hpp : 654 and 502
//

namespace rt::redis
{

template < class AsyncStream
         , class Handler>
struct read_resp_op {
   AsyncStream& stream;
   Handler handler;
   std::string* data = nullptr;
   int start = 0;
   int counter = 1;
   bool bulky_str_read = false;
   std::vector<std::string> res;

   using allocator_type =
      net::associated_allocator_t<Handler>;

   using executor_type =
      net::associated_executor_t<
         Handler, decltype(std::declval<AsyncStream&>().get_executor())>;

   read_resp_op( AsyncStream& stream_
               , std::string* data_
               , Handler handler_)
   : stream(stream_)
   , handler(std::move(handler_))
   , data(data_)
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
            net::async_read_until( stream, net::dynamic_buffer(*data)
                                 , "\r\n", std::move(*this));
            return; default:

            if (ec || n < 3) {
               handler(ec, {});
               return;
            }

            auto str_flag = false;
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
                           str_flag = true;
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
               handler({}, res);
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
BOOST_ASIO_INITFN_RESULT_TYPE( CompletionToken
                             , void(boost::beast::error_code))
async_read_resp( AsyncStream& s
               , std::string* data
               , CompletionToken&& handler)
{
   // TODO: Write a similar macro to check the handler signature.
   //BOOST_ASIO_READ_HANDLER_CHECK(CompletionToken, handler) type_check;

   using read_handler_signature =
      void ( boost::system::error_code const&
           , std::vector<std::string> const&);

   net::async_completion< CompletionToken
                        , read_handler_signature
                        > init {handler};

   using handler_type = 
      read_resp_op< AsyncStream
                  , BOOST_ASIO_HANDLER_TYPE( CompletionToken
                                           , read_handler_signature)
                  >;

   handler_type {s, data, init.completion_handler}({}, 0, 1);

   return init.result.get();
}

}


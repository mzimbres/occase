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
class read_resp_op {
private:
   static std::string_view constexpr delim {"\r\n"};
   AsyncStream& stream;
   Handler handler;
   std::string* data;
   std::vector<std::string> res;
   int counter;
   bool bulky_str_read;

public:
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

   void operator()( boost::system::error_code ec, std::size_t n
                  , bool start = false)
   {
      if (start) {
         counter = 1;
         bulky_str_read = false;
         net::async_read_until( stream, net::dynamic_buffer(*data)
                              , delim, std::move(*this));
         return;
      }

      if (ec || n < 3) {
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
         handler({}, res);
         return;
      }

      bulky_str_read = foo;
      net::async_read_until( stream, net::dynamic_buffer(*data), delim
                           , std::move(*this));
   }
};

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

   net::async_completion< CompletionToken
                        , void ( boost::system::error_code const&
                               , std::vector<std::string> const&)
                         > init {handler};

   
   read_resp_op< AsyncStream
               , BOOST_ASIO_HANDLER_TYPE( CompletionToken
                                        , void ( boost::system::error_code const&
                                               , std::vector<std::string> const&))
               >(s, data, init.completion_handler)({}, 0, true);

   return init.result.get();
}

}


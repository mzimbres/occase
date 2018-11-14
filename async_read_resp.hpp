#pragma once

#include <boost/asio.hpp>
#include <boost/asio/async_result.hpp>

#include "resp.hpp"

// TODO: Improve this according to
// boost_1_67_0/boost/asio/impl/read.hpp : 654 and 502
//

namespace rt
{

template <class ReadHandler>
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
   , handler(std::move(handler_))
   , data(data_)
   { }

   read_resp_op(read_resp_op const& other)
   : stream(other.stream)
   , handler(other.handler)
   , data(other.data)
   , res(other.res)
   , counter(other.counter)
   , bulky_str_read(other.bulky_str_read)
   { }

   read_resp_op(read_resp_op&& other)
   : stream(other.stream)
   , handler(std::move(other.handler))
   , data(other.data)
   , res(std::move(other.res))
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
      asio::async_read_until( stream, asio::dynamic_buffer(*data), delim
                            , std::move(*this));
   }
};

template <class ReadHandler>
void async_read_resp( boost::asio::ip::tcp::socket& s
                    , std::string* data, ReadHandler handler)
{
   // TODO: Write a similar macro to check the handler signature.
   //BOOST_ASIO_READ_HANDLER_CHECK(ReadHandler, handler) type_check;

   using foo_type = 
   asio::async_completion< ReadHandler
                         , void ( boost::system::error_code const&
                                , std::vector<std::string> const&)
                         >;

   foo_type init(handler);

   read_resp_op< typename foo_type::completion_handler_type
               >(s, data, init.completion_handler)({}, 0, true);
}

}


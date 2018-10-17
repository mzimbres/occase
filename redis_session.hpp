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

#include <boost/asio.hpp>
#include <boost/asio/bind_executor.hpp>
#include <boost/asio/ip/tcp.hpp>

namespace aedis
{

struct interaction {
   std::string cmd;
   std::function<void(boost::system::error_code, std::string)> action;
};

class redis_session {
private:
   using value_type = std::string::value_type;
   using traits_type = std::string::traits_type;
   using allocator_type = std::string::allocator_type;

   static std::string_view constexpr delim {"\r\n"};
   boost::asio::ip::tcp::socket socket;
   std::string data;
   std::queue<interaction> write_queue;
   boost::asio::ip::tcp::resolver::results_type endpoints;
   bool waiting_response = false;
   int counter = 1;
   bool bulky_str_read = false;

   void do_write(interaction i);
   void do_read_resp();
   void do_close();

   void on_connect(boost::system::error_code ec);
   void on_resp();
   void on_resp_chunk(boost::system::error_code ec, std::size_t n);
   void on_write(boost::system::error_code ec, std::size_t n);

public:
   redis_session( boost::asio::io_context& ioc_
                , boost::asio::ip::tcp::resolver::results_type endpoints_)
   : socket(ioc_)
   , endpoints(endpoints_)
   { }

   void run();
   void send(interaction i);
   void close();
};

}


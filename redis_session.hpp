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

#include "stream.hpp"

namespace aedis
{

struct interaction {
   std::string cmd;
   std::function<void(boost::system::error_code, std::string)> action;
   bool sent = false;
};

class redis_session {
private:
   using value_type = std::string::value_type;
   using traits_type = std::string::traits_type;
   using allocator_type = std::string::allocator_type;

   stream<boost::asio::ip::tcp::socket, 3> rs;
   std::string result;
   boost::asio::dynamic_string_buffer< value_type, traits_type
                                     , allocator_type
                                     > buffer;
   std::queue<interaction> write_queue;
   boost::asio::ip::tcp::resolver::results_type endpoints;

   void do_write(interaction i);
   void do_read();
   void do_close();

   void on_connect(boost::system::error_code ec);
   void on_read(boost::system::error_code ec, std::size_t n);
   void on_write(boost::system::error_code ec, std::size_t n);

public:
   redis_session( boost::asio::io_context& ioc_
                , boost::asio::ip::tcp::resolver::results_type endpoints_)
   : rs(ioc_)
   , buffer(result)
   , endpoints(endpoints_)
   { }

   void run();
   void send(interaction i);
   void close();
};

}


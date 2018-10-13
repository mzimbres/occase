#pragma once

#include <array>
#include <queue>
#include <vector>
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

class redis_session :
  public std::enable_shared_from_this<redis_session> {
private:
   stream<boost::asio::ip::tcp::socket, 3> rs;
   std::vector<char> result;
   boost::asio::dynamic_vector_buffer<char, std::allocator<char>> buffer;
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
   , buffer(result)
   , endpoints(endpoints_)
   { }

   void run();
   void write(std::string msg);
   void close();
};

}


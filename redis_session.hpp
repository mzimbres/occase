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

struct interaction {
   std::string cmd;
   std::function< void ( boost::system::error_code ec
                       , std::vector<char>)> action;
   bool sent = false;
};

class redis_session :
  public std::enable_shared_from_this<redis_session> {
private:
   stream<boost::asio::ip::tcp::socket, 3> rs;
   std::vector<char> result;
   boost::asio::dynamic_vector_buffer< std::vector<char>::value_type
                                     , std::vector<char>::allocator_type
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


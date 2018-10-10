#pragma once

#include <queue>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>

#include <boost/asio.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/bind_executor.hpp>
#include <boost/asio/ip/tcp.hpp>

namespace aedis
{

class redis_session :
  public std::enable_shared_from_this<redis_session> {
private:
   boost::asio::ip::tcp::socket socket;
   boost::asio::strand<boost::asio::io_context::executor_type> strand;
   static constexpr auto msg_size = 3;
   std::vector<char> message {msg_size};
   std::vector<char> result;
   std::queue<std::string> write_queue;
   boost::asio::ip::tcp::resolver::results_type endpoints;

   void on_connect(boost::system::error_code ec);
   void do_read(boost::system::error_code ec, std::size_t n);
   void do_write(std::string msg);
   void on_write(boost::system::error_code ec, std::size_t n);
   void do_close();

public:
   redis_session( boost::asio::io_context& ioc_
                , boost::asio::ip::tcp::resolver::results_type endpoints_)
   : socket(ioc_)
   , strand(socket.get_executor())
   , endpoints(endpoints_)
   { }

   void run();
   void write(std::string msg);
   void close();
};

}


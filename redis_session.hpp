#pragma once

#include <queue>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>

#include <boost/asio.hpp>
#include <boost/asio/bind_executor.hpp>
#include <boost/asio/ip/tcp.hpp>

namespace aedis
{

class stream {
public:
   using next_layer_type = boost::asio::ip::tcp::socket;
private:
   next_layer_type socket;
public:
   next_layer_type& next_layer() {return socket;}
   next_layer_type const& next_layer() const {return socket;}
};

class redis_session :
  public std::enable_shared_from_this<redis_session> {
private:
   boost::asio::ip::tcp::socket socket;
   static constexpr auto msg_size = 3;
   std::vector<char> message {msg_size};
   std::vector<char> result;
   std::queue<std::string> write_queue;
   boost::asio::ip::tcp::resolver::results_type endpoints;

   void on_connect(boost::system::error_code ec);
   void on_read_some(boost::system::error_code ec, std::size_t n);
   void on_read();
   void do_write(std::string msg);
   void on_write(boost::system::error_code ec, std::size_t n);
   void do_close();
   void do_read_some();

public:
   redis_session( boost::asio::io_context& ioc_
                , boost::asio::ip::tcp::resolver::results_type endpoints_)
   : socket(ioc_)
   , endpoints(endpoints_)
   { }

   void run();
   void write(std::string msg);
   void close();
};

}


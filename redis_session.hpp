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

#include "config.hpp"

namespace aedis
{

// TODO: Use a strategy similar to 
// boost_1_67_0/boost/asio/impl/read.hpp : 654 and 502

struct redis_session_cf {
   std::string host;
   std::string port;
};

using redis_handler_type =
   std::function<void(boost::system::error_code, std::vector<std::string>)>;

class redis_session {
private:
   static std::string_view constexpr delim {"\r\n"};

   redis_session_cf cf;
   tcp::resolver resolver;
   boost::asio::ip::tcp::socket socket;
   std::string data;
   std::vector<std::string> res;
   std::queue<std::string> write_queue;
   bool waiting_response = false;
   redis_handler_type msg_handler = [](auto, auto){};

   void start_reading_resp();

   void on_resolve( boost::system::error_code ec
                  , tcp::resolver::results_type results);
   void on_connect( boost::system::error_code ec
                  , asio::ip::tcp::endpoint const& endpoint);
   void on_resp(boost::system::error_code ec);
   void on_resp_chunk( boost::system::error_code ec
                     , std::size_t n, int counter
                     , bool bulky_str_read);
   void on_write(boost::system::error_code ec, std::size_t n);

public:
   redis_session(redis_session_cf cf_, asio::io_context& ioc)
   : cf(cf_)
   , resolver(ioc) 
   , socket(ioc)
   { }

   void run();
   void send(std::string msg);
   void close();
   void set_msg_handler(redis_handler_type handler)
   { msg_handler = std::move(handler);};
};

}


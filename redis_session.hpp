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
   std::function<void(boost::system::error_code, std::string)>;

struct interaction {
   std::string cmd;
   redis_handler_type action;
};

class redis_session {
private:
   static std::string_view constexpr delim {"\r\n"};

   redis_session_cf cf;
   tcp::resolver resolver;
   boost::asio::ip::tcp::socket socket;
   std::string data;
   std::queue<interaction> write_queue;
   bool waiting_response = false;
   redis_handler_type sub_handler = [](auto, auto){};

   void start_reading_resp();

   void on_resolve( boost::system::error_code ec
                  , tcp::resolver::results_type results);
   void on_connect( boost::system::error_code ec
                  , asio::ip::tcp::endpoint const& endpoint);
   void on_resp(boost::system::error_code ec);
   void on_resp_chunk( boost::system::error_code ec
                     , std::size_t n
                     , int counter
                     , bool bulky_str_read
                     , std::size_t pos);
   void on_write(boost::system::error_code ec, std::size_t n);

public:
   redis_session(redis_session_cf cf_, asio::io_context& ioc)
   : cf(cf_)
   , resolver(ioc) 
   , socket(ioc)
   { }

   void run();
   void send(interaction i);
   void close();
   void set_sub_handler(redis_handler_type handler)
   {sub_handler = handler;};
};

}


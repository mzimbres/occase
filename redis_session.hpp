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

struct redis_session_cf {
   std::string host;
   std::string port;
};

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

   redis_session_cf cf;
   tcp::resolver resolver;
   boost::asio::ip::tcp::socket socket;
   std::string data;
   std::queue<interaction> write_queue;
   bool waiting_response = false;
   int counter = 1;
   bool bulky_str_read = false;

   void do_write(interaction i);
   void start_reading_resp();
   void do_close();

   void on_resolve( boost::system::error_code ec
                  , tcp::resolver::results_type results);
   void on_connect( boost::system::error_code ec
                  , asio::ip::tcp::endpoint const& endpoint);
   void on_resp();
   void on_resp_chunk(boost::system::error_code ec, std::size_t n);
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
};

}


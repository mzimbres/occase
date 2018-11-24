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
#include <type_traits>

#include <boost/asio.hpp>
#include <boost/asio/bind_executor.hpp>
#include <boost/asio/ip/tcp.hpp>

#include "resp.hpp"
#include "config.hpp"

namespace rt
{

enum class request
{ get_menu
, lpop
, lrange
, ping
, rpush
, publish
, set
, subscribe
, unsubscribe
, unsolicited // No a redis cmd. Received in subscribe mode.
};

struct req_data {
   request cmd = request::unsolicited;
   std::string msg;
   std::string user_id;
};

struct redis_session_cf {
   std::string host;
   std::string port;
};

class redis_session {
public:
   using redis_on_msg_handler_type =
      std::function<void ( boost::system::error_code const&
                         , std::vector<std::string> const&
                         , req_data const&)>;

private:
   redis_session_cf cf;
   net::ip::tcp::resolver resolver;
   net::ip::tcp::socket socket;
   std::string data;
   std::queue<req_data> write_queue;
   redis_on_msg_handler_type on_msg_handler = [](auto, auto, auto) {};

   void start_reading_resp();

   void on_resolve( boost::system::error_code ec
                  , net::ip::tcp::resolver::results_type results);
   void on_connect( boost::system::error_code ec
                  , net::ip::tcp::endpoint const& endpoint);
   void on_resp( boost::system::error_code const& ec
               , std::vector<std::string> const& res);
   void on_write( boost::system::error_code ec
                , std::size_t n);

public:
   redis_session(redis_session_cf cf_, net::io_context& ioc)
   : cf(cf_)
   , resolver(ioc) 
   , socket(ioc)
   { }

   void run();
   void send(req_data req);
   void close();
   void set_on_msg_handler(redis_on_msg_handler_type handler)
   { on_msg_handler = std::move(handler);};
};

}


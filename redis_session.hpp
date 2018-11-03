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

namespace aedis
{

struct redis_session_cf {
   std::string host;
   std::string port;
};

class redis_session {
public:
   using redis_on_msg_handler_type =
      std::function<void ( boost::system::error_code const&
                         , std::vector<std::string> const&
                         , redis_req const&)>;

private:
   redis_session_cf cf;
   tcp::resolver resolver;
   tcp::socket socket;
   std::string data;
   std::queue<redis_req> write_queue;
   redis_on_msg_handler_type on_msg_handler = [](auto, auto, auto) {};

   void start_reading_resp();

   void on_resolve( boost::system::error_code ec
                  , tcp::resolver::results_type results);
   void on_connect( boost::system::error_code ec
                  , asio::ip::tcp::endpoint const& endpoint);
   void on_resp( boost::system::error_code const& ec
               , std::vector<std::string> const& res);
   void on_write( boost::system::error_code ec
                , std::size_t n);

public:
   redis_session(redis_session_cf cf_, asio::io_context& ioc)
   : cf(cf_)
   , resolver(ioc) 
   , socket(ioc)
   { }

   void run();
   void send(redis_req req);
   void close();
   void set_on_msg_handler(redis_on_msg_handler_type handler)
   { on_msg_handler = std::move(handler);};
};

}


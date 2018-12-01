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
#include <boost/asio/steady_timer.hpp>

#include "resp.hpp"
#include "config.hpp"
#include "async_read_resp.hpp"

namespace rt::redis
{

enum class request
{ get_menu
, retrieve_msgs
, store_msg
, publish
, subscribe
, unsubscribe
, unsolicited
};

struct req_data {
   request cmd = request::unsolicited;
   std::string msg;
   std::string user_id;
};

struct session_cf {
   std::string host;
   std::string port;
};

class session {
public:
   using on_conn_handler_type = std::function<void(session&)>;

   using msg_handler_type =
      std::function<void ( boost::system::error_code const&
                         , std::vector<std::string> const&
                         , req_data const&)>;
private:
   session_cf cf;
   net::ip::tcp::resolver resolver;
   net::ip::tcp::socket socket;
   net::steady_timer timer;
   resp_buffer buffer;
   std::queue<req_data> write_queue;
   msg_handler_type msg_handler =
      []( auto const&, auto const&, auto const&) {};

   on_conn_handler_type on_conn_handler = [](auto&) {};

   void start_reading_resp();

   void on_resolve( boost::system::error_code const& ec
                  , net::ip::tcp::resolver::results_type results);
   void on_connect( boost::system::error_code const& ec
                  , net::ip::tcp::endpoint const& endpoint);
   void on_resp(boost::system::error_code const& ec);
   void on_write( boost::system::error_code ec
                , std::size_t n);

public:
   session(session_cf cf_, net::io_context& ioc)
   : cf(cf_)
   , resolver(ioc) 
   , socket(ioc)
   , timer(ioc, std::chrono::steady_clock::time_point::max())
   { }

   void run();
   void send(req_data req);
   void close();

   void set_msg_handler(msg_handler_type handler)
   { msg_handler = std::move(handler);};

   void set_on_conn_handler(on_conn_handler_type handler)
   { on_conn_handler = std::move(handler);};
};

}


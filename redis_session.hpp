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

#include "config.hpp"
#include "async_read_resp.hpp"

namespace rt::redis
{

struct session_cf {
   std::string host;
   std::string port;

   std::chrono::milliseconds conn_retry_interval {500};

   // We have to restrict the length of the pipeline to not block
   // redis for long periods.
   int max_pipeline_size {10000};
};

class session {
public:
   using on_conn_handler_type = std::function<void()>;

   using on_disconnect_handler_type =
      std::function<void(boost::system::error_code const&)>;

   using msg_handler_type =
      std::function<void( boost::system::error_code const&
                        , std::vector<std::string>)>;

private:
   std::string id;
   session_cf cf;
   net::ip::tcp::resolver resolver;
   net::ip::tcp::socket socket;
   net::steady_timer timer;
   resp_buffer buffer;
   std::queue<std::string> msg_queue;
   int pipeline_counter = 0;

   msg_handler_type on_msg_handler = [](auto const&, auto) {};
   on_conn_handler_type on_conn_handler = [](){};
   on_disconnect_handler_type on_disconnect_handler = [](auto const&){};

   void start_reading_resp();

   void on_resolve( boost::system::error_code const& ec
                  , net::ip::tcp::resolver::results_type results);
   void on_connect( boost::system::error_code const& ec
                  , net::ip::tcp::endpoint const& endpoint);
   void on_resp(boost::system::error_code const& ec);
   void on_write( boost::system::error_code ec
                , std::size_t n);

public:
   session(session_cf cf_, net::io_context& ioc, std::string id_);

   void set_on_conn_handler(on_conn_handler_type handler)
      { on_conn_handler = std::move(handler);};

   void set_on_disconnect_handler(on_disconnect_handler_type handler)
      { on_disconnect_handler = std::move(handler);};

   void set_msg_handler(msg_handler_type handler)
      { on_msg_handler = std::move(handler);};

   void send(std::string req);
   void close();
   void run();
};

}


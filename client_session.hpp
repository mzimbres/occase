#pragma once

#include "config.hpp"

#include <set>
#include <thread>
#include <vector>
#include <chrono>
#include <sstream>
#include <functional>

#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/steady_timer.hpp>

struct client_options {
   std::string host;
   std::string port;
   std::string tel;
   bool interative;
   int create_n_groups;
   std::chrono::milliseconds create_groups_int;
   int number_of_joins;
   std::chrono::milliseconds joins_interval;
};

class client_session : public std::enable_shared_from_this<client_session> {
private:
   using work_type =
      boost::asio::executor_work_guard<
         boost::asio::io_context::executor_type>;

   tcp::resolver resolver;
   boost::asio::steady_timer timer;
   websocket::stream<tcp::socket> ws;
   boost::beast::multi_buffer buffer;
   work_type work;
   std::string text;

   client_options op;
   int id = -1;
   std::set<int> groups;

   void write(std::string msg);
   void async_close();
   void on_resolve( boost::system::error_code ec
                  , tcp::resolver::results_type results);
   void on_connect( boost::system::error_code ec
                  , tcp::resolver::results_type results);
   void on_handshake( boost::system::error_code ec
                    , tcp::resolver::results_type results);
   void do_read(tcp::resolver::results_type results);
   void on_write( boost::system::error_code ec
                , std::size_t bytes_transferred);
   void on_read( boost::system::error_code ec
               , std::size_t bytes_transferred
               , tcp::resolver::results_type results);
   void on_close(boost::system::error_code ec);
   void send_msg(std::string msg);
   void login_ack_handler(json j);
   void create_group_ack_handler(json j);
   void join_group_ack_handler(json j);
   void message_handler(json j);
   void async_connect(tcp::resolver::results_type results);

public:
   explicit
   client_session( boost::asio::io_context& ioc
                 , client_options op_);
   void login();
   void create_group();
   void join_group();
   void send_group_msg(std::string msg);
   void send_user_msg(std::string msg);
   void exit();
   void run();
};


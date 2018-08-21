#pragma once

#include "config.hpp"
#include "json_utils.hpp"

#include <set>
#include <queue>
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
   std::chrono::milliseconds interval;
   int number_of_group_msgs;
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
   std::queue<std::string> msg_queue;

   client_options op;
   user_bind bind;
   std::set<int> groups;

   int number_of_logins = 5;
   int number_of_dropped_logins = number_of_logins;

   int number_of_valid_create_groups = 10;
   int number_of_create_groups = 8;
   int number_of_dropped_create_groups = number_of_create_groups - 2;

   int number_of_valid_joins = 10;
   int number_of_joins = 4;

   int number_of_valid_group_msgs = 10;
   int number_of_group_msgs = 4;

   void async_close();
   void write(std::string msg);
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
   void on_login_ack(json j);
   void on_create_group_ack(json j);
   void on_join_group_ack(json j);
   void on_send_group_msg_ack(json j);
   void on_message(json j);
   void async_connect(tcp::resolver::results_type results);
   void login();
   void create_group();
   void join_group();
   void send_group_msg();
   void send_user_msg();
   void close();

public:
   explicit
   client_session( boost::asio::io_context& ioc
                 , client_options op_);
   void prompt_login();
   void prompt_create_group();
   void prompt_join_group();
   void prompt_send_group_msg();
   void prompt_send_user_msg();
   void prompt_close();
   void run();
};


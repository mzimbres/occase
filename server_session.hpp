#pragma once

#include <memory>

#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio/bind_executor.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/ip/tcp.hpp>

#include "config.hpp"
#include "server_data.hpp"

class server_session :
   public std::enable_shared_from_this<server_session> {
private:
   websocket::stream<tcp::socket> ws;
   boost::asio::strand<
      boost::asio::io_context::executor_type> strand;
   boost::beast::multi_buffer buffer;
   std::shared_ptr<server_data> sd;

   void on_accept(boost::system::error_code ec);
   void do_read();

   void on_read( boost::system::error_code ec
               , std::size_t bytes_transferred);

   void on_write( boost::system::error_code ec
                , std::size_t bytes_transferred);

public:
   explicit
   server_session( tcp::socket socket
                 , std::shared_ptr<server_data> sd_);

   void run();
   void write(std::string msg);
};



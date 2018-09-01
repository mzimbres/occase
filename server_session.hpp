#pragma once

#include <memory>

#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio/bind_executor.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/ip/tcp.hpp>

#include "config.hpp"
#include "server_mgr.hpp"

class server_session :
   public std::enable_shared_from_this<server_session> {
private:
   websocket::stream<tcp::socket> ws;
   boost::asio::strand<
      boost::asio::io_context::executor_type> strand;
   boost::beast::multi_buffer buffer;
   std::shared_ptr<server_mgr> sd;

   void do_read();
   void do_close();

   void on_read( boost::system::error_code ec
               , std::size_t bytes_transferred);

   void on_close(boost::system::error_code ec);

   void on_accept(boost::system::error_code ec);

   void on_write( boost::system::error_code ec
                , std::size_t bytes_transferred);

   // A value greater than zero means a valid user index.
   // -1: We should close the conection.
   // -2: We are awaiting for an sms.
   int user_idx = -1;

   // This is only non empty if we are waiting for a user sms.
   std::string sms;

public:
   explicit
   server_session( tcp::socket socket
                 , std::shared_ptr<server_mgr> sd_);

   void run();
   void write(std::string msg);
   void set_sms(std::string sms_) {sms = std::move(sms_);}
   auto const& get_sms() {return sms;}
};


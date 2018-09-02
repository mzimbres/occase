#pragma once

#include <set>
#include <queue>
#include <thread>
#include <chrono>
#include <vector>
#include <chrono>
#include <memory>
#include <sstream>
#include <functional>

#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/steady_timer.hpp>

#include "config.hpp"
#include "json_utils.hpp"

struct client_options {
   std::string host;
   std::string port;
};

template <class Mgr>
class client_session :
   public std::enable_shared_from_this<client_session<Mgr>> {
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

   Mgr& mgr;

   void do_close();
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
   void do_connect(tcp::resolver::results_type results);

public:
   explicit
   client_session( boost::asio::io_context& ioc
                 , client_options op_
                 , Mgr& m);

   void write(std::string msg);
   void run();
};

inline
void fail_tmp(boost::system::error_code ec, char const* what)
{
   std::cerr << what << ": " << ec.message() << "\n";
}

template <class Mgr>
void client_session<Mgr>::on_read( boost::system::error_code ec
                                 , std::size_t bytes_transferred
                                 , tcp::resolver::results_type results)
{
   try {
      boost::ignore_unused(bytes_transferred);

      if (ec) {
         if (ec == websocket::error::closed) {
            // This means the session has been close, likely by the
            // server. Now we ask the manager what to do, should we
            // try to reconnect or are we done.
            if (mgr.on_closed(ec) == -1) {
               // We are done.
               //std::cout << "Leaving on read 1." << std::endl;
               work.reset();
               return;
            }

            // The manager wants us to reconnect to continue with its
            // tests.
            buffer.consume(buffer.size());
            //std::cout << "Reconnecting." << std::endl;

            // I am not seeing any need of a timer here. Perhaps it
            // should be removed.
            timer.expires_after(std::chrono::milliseconds{100});

            auto handler = [results, p = this->shared_from_this()](auto ec)
            { p->do_connect(results); };

            timer.async_wait(handler);
            //std::cout << "Leaving on read 2." << std::endl;
            return;
         }

         if (ec == boost::asio::error::operation_aborted) {
            // I am unsure by this may be caused by a do_close.
            //std::cout << "Leaving on read 3." << std::endl;
            timer.cancel();
            work.reset();
            return;
         }

         //std::cout << "Leaving on read 4." << std::endl;
         work.reset();
         return;
      }

      json j;
      std::stringstream ss;
      ss << boost::beast::buffers(buffer.data());
      ss >> j;
      buffer.consume(buffer.size());
      //auto str = ss.str();
      //std::cout << "Received: " << str << std::endl;

      if (mgr.on_read(j, this->shared_from_this()) == -1) {
         do_close();
         return;
      }

   } catch (std::exception const& e) {
      std::cerr << "Server error. Please fix." << std::endl;
      std::cerr << "Error: " << e.what() << std::endl;
   }

   do_read(results);
}

template <class Mgr>
client_session<Mgr>::client_session( boost::asio::io_context& ioc
                                   , client_options op_
                                   , Mgr& m)
: resolver(ioc)
, timer(ioc)
, ws(ioc)
, work(boost::asio::make_work_guard(ioc))
, op(std::move(op_))
, mgr(m)
{ }

template <class Mgr>
void client_session<Mgr>::write(std::string msg)
{
   //std::cout << "Sending: " << msg << std::endl;
   text = std::move(msg);

   auto handler = [p = this->shared_from_this()](auto ec, auto res)
   { p->on_write(ec, res); };

   ws.async_write(boost::asio::buffer(text), handler);
}

template <class Mgr>
void client_session<Mgr>::do_close()
{
   //std::cout << "do_close" << std::endl;
   auto handler = [p = this->shared_from_this()](auto ec)
   { p->on_close(ec); };

   ws.async_close(websocket::close_code::normal, handler);
}

template <class Mgr>
void client_session<Mgr>::on_resolve( boost::system::error_code ec
                                    , tcp::resolver::results_type results)
{
   if (ec)
      return fail_tmp(ec, "resolve");

   do_connect(results);
}

template <class Mgr>
void client_session<Mgr>::do_connect(tcp::resolver::results_type results)
{
   //std::cout  << "Trying to connect." << std::endl;
   auto handler =
      [results, p = this->shared_from_this()](auto ec, auto Iterator)
   { p->on_connect(ec, results); };

   boost::asio::async_connect(ws.next_layer(), results.begin(),
      results.end(), handler);
}

template <class Mgr>
void
client_session<Mgr>::on_connect( boost::system::error_code ec
                               , tcp::resolver::results_type results)
{
   if (ec) {
      timer.expires_after(std::chrono::milliseconds{10});

      auto handler = [results, p = this->shared_from_this()](auto ec)
      { p->do_connect(results); };

      timer.async_wait(handler);
      return;
   }

   //std::cout << "Connection stablished." << std::endl;

   auto handler = [p = this->shared_from_this(), results](auto ec)
   { p->on_handshake(ec, results); };

   // Perform the websocket handshake
   ws.async_handshake(op.host, "/", handler);
}

template <class Mgr>
void
client_session<Mgr>::on_handshake( boost::system::error_code ec
                                 , tcp::resolver::results_type results)
{
   //std::cout << "on_handshake" << std::endl;
   if (ec)
      return fail_tmp(ec, "handshake");

   // This function must be called before we begin to write commands
   // so that we can receive the acks from the server.
   do_read(results);

   if (mgr.on_handshake(this->shared_from_this()) == -1) {
      do_close();
   }
}

template <class Mgr>
void client_session<Mgr>::do_read(tcp::resolver::results_type results)
{
   auto handler = [p = this->shared_from_this(), results](auto ec, auto res)
   { p->on_read(ec, res, results); };

   ws.async_read(buffer, handler);
}

template <class Mgr>
void client_session<Mgr>::on_write( boost::system::error_code ec
                                  , std::size_t bytes_transferred)
{
   boost::ignore_unused(bytes_transferred);

   mgr.on_write(this->shared_from_this()); // TODO: Use the return value?

   if (ec)
      return fail_tmp(ec, "write");
}

template <class Mgr>
void client_session<Mgr>::on_close(boost::system::error_code ec)
{
   if (ec)
      return fail_tmp(ec, "close");

   //std::cout << "Connection closed gracefully" << std::endl;
   work.reset();
}

template <class Mgr>
void client_session<Mgr>::run()
{
   auto handler = [p = this->shared_from_this()](auto ec, auto res)
   { p->on_resolve(ec, res); };

   // Look up the domain name
   resolver.async_resolve(op.host, op.port, handler);
}


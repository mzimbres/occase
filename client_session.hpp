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

struct client_session_config {
   std::string host;
   std::string port;
   std::chrono::seconds handshake_timeout;
   std::chrono::seconds auth_timeout;
};

template <class Mgr>
class client_session :
   public std::enable_shared_from_this<client_session<Mgr>> {
private:
   tcp::resolver resolver;
   boost::asio::steady_timer timer;
   websocket::stream<tcp::socket> ws;
   boost::beast::multi_buffer buffer;
   std::string text;
   client_session_config op;
   std::queue<std::string> msg_queue;
   bool closing = false;

   Mgr& mgr;

   int sent_msgs = 0;
   int recv_msgs = 0;

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
   void do_write(std::string msg);

public:
   explicit
   client_session( boost::asio::io_context& ioc
                 , client_session_config op_
                 , Mgr& m);

   ~client_session()
   {
      //std::cout << "Session in destruction." << std::endl;
   }

   void run();
   void send_msg(std::string msg);
   auto const& get_mgr() const noexcept {return mgr;}
   auto get_sent_msgs() const noexcept {return sent_msgs;}
   auto get_recv_msgs() const noexcept {return recv_msgs;}
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
   std::string str;
   //try {
      boost::ignore_unused(bytes_transferred);

      if (ec) {
         if (ec == websocket::error::closed) {
            // This means the session has been closed by the server.
            if (mgr.on_closed(ec) == -1) {
               //std::cout << "Cancelling timer." << std::endl;
               timer.cancel();
               return;
            }

            // The manager wants us to reconnect to continue with its
            // tests.
            buffer.consume(buffer.size());

            std::cout << "Leaving on read 2." << std::endl;
            return;
         }

         if (ec == boost::asio::error::operation_aborted) {
            // I am unsure by this may be caused by a do_close.
            std::cout << "Leaving on read 3." << std::endl;
            timer.cancel();
            return;
         }

         //std::cout << "Leaving on read 4." << std::endl;
         return;
      }

      ++recv_msgs;
      str = boost::beast::buffers_to_string(buffer.data());
      buffer.consume(std::size(buffer));
      json j = json::parse(str);
      //std::cout << "Received: " << str << std::endl;

      if (mgr.on_read(j, this->shared_from_this()) == -1) {
         do_close();
         return;
      }

   //} catch (std::exception const& e) {
   //   std::cerr << "Server error, please fix: " << str << std::endl;
   //   std::cerr << "Error: " << e.what() << std::endl;
   //   return;
   //}

   do_read(results);
}

template <class Mgr>
client_session<Mgr>::client_session( boost::asio::io_context& ioc
                                   , client_session_config op_
                                   , Mgr& m)
: resolver(ioc)
, timer(ioc)
, ws(ioc)
, op(std::move(op_))
, mgr(m)
{ }

template <class Mgr>
void client_session<Mgr>::send_msg(std::string msg)
{
   ++sent_msgs;
   auto is_empty = std::empty(msg_queue);
   msg_queue.push(std::move(msg));

   if (is_empty)
      do_write(msg_queue.front());
}

template <class Mgr>
void client_session<Mgr>::do_write(std::string msg)
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
   if (closing)
      return;

   closing = true;

   //std::cout << "do_close" << std::endl;
   auto handler = [p = this->shared_from_this()](auto ec)
   { p->on_close(ec); };

   ws.async_close(websocket::close_code::normal, handler);
}

template <class Mgr>
void client_session<Mgr>::on_close(boost::system::error_code ec)
{
   if (ec)
      fail_tmp(ec, "close");

   //std::cout << "Connection closed gracefully" << std::endl;
}

template <class Mgr>
void client_session<Mgr>::on_resolve( boost::system::error_code ec
                                    , tcp::resolver::results_type results)
{
   if (ec)
      return fail_tmp(ec, "resolve");

   auto this_obj = this->shared_from_this();
   auto handler = [results, p = this_obj](auto ec, auto Iterator)
   { p->on_connect(ec, results); };

   boost::asio::async_connect( ws.next_layer(), results.begin()
                             , results.end(), handler);
}

template <class Mgr>
void
client_session<Mgr>::on_connect( boost::system::error_code ec
                               , tcp::resolver::results_type results)
{
   if (ec)
      return fail_tmp(ec, "resolve");

   if (mgr.on_connect() == -1) {
      // The -1 means we are are testing if the server will timeout
      // our connection if the handshake lasts too long. So here we
      // wont proceed with the handshake but set a timer to throw an
      // exception on timeout. If the server closes the connection the
      // timer will be canceled.
      timer.expires_after(op.handshake_timeout);

      auto handler = [p = this->shared_from_this()](auto ec)
      {
         if (ec) {
            if (ec == boost::asio::error::operation_aborted) {
               // The timer has been successfully canceled.
               //std::cout << "Timer successfully canceled." << std::endl;
               return;
            }
         }

         throw std::runtime_error("client_session<Mgr>::on_timer: fail.");
      };

      timer.async_wait(handler);

      // Now we post the handler that will cancel the timer when the
      // server gives up the handshake by closing the connection.
      auto handler2 = [p = this->shared_from_this()](auto ec, auto n)
      {
         if (ec == boost::asio::error::eof) {
            p->timer.cancel();
            //std::cout << "Timer canceled, thanks." << std::endl;
            return;
         }

         std::cout << "Unexpected error." << std::endl;
         std::cout << "Bytes transferred: " << n << std::endl;
      };

      char dummy[32];
      ws.next_layer().async_receive( boost::asio::buffer( &dummy[0]
                                                        , std::size(dummy))
                                   , 0, handler2);
      return;
   }

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
      throw std::runtime_error(ec.message());

   // This function must be called before we begin to write commands
   // so that we can receive the acks from the server.
   do_read(results);

   auto const r = mgr.on_handshake(this->shared_from_this());
   if (r == -1) {
      // This means the mgr wants us to set a timer that will fire if
      // not canceled on timer. The canceling should happen on the
      // on_read when the server gracefully closes the connection. It
      // should do so since we are not sending any auth or login
      // command.

      timer.expires_after(op.auth_timeout);

      auto handler = [p = this->shared_from_this()](auto ec)
      {
         if (ec) {
            if (ec == boost::asio::error::operation_aborted) {
               // The timer has been successfully canceled.
               //std::cout << "Timer successfully canceled." << std::endl;
               return;
            }
         }

         throw std::runtime_error("client_session<Mgr>::on_handshake: fail.");
      };

      timer.async_wait(handler);
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

   msg_queue.pop();
   if (msg_queue.empty())
      return; // No more message to send to the client.

   do_write(msg_queue.front());

   if (ec)
      fail_tmp(ec, "write");
}

template <class Mgr>
void client_session<Mgr>::run()
{
   auto handler = [p = this->shared_from_this()](auto ec, auto res)
   { p->on_resolve(ec, res); };

   // Look up the domain name
   resolver.async_resolve(op.host, op.port, handler);
}


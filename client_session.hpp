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

namespace rt
{

struct client_session_cf {
   std::string host;
   std::string port;
   std::chrono::seconds handshake_timeout;
   std::chrono::seconds auth_timeout;
};

template <class Mgr>
class client_session :
   public std::enable_shared_from_this<client_session<Mgr>> {
private:
   net::ip::tcp::resolver resolver;
   net::steady_timer timer;
   beast::websocket::stream<net::ip::tcp::socket> ws;
   beast::multi_buffer buffer;
   std::string text;
   client_session_cf op;
   std::queue<std::string> msg_queue;
   bool closing = false;
   std::string receive_buffer;

   Mgr mgr;

   void do_close();
   void on_resolve( boost::system::error_code ec
                  , net::ip::tcp::resolver::results_type results);
   void on_connect( boost::system::error_code ec
                  , net::ip::tcp::endpoint const& endpoint);
   void on_handshake( boost::system::error_code ec);
   void do_read();
   void on_write( boost::system::error_code ec
                , std::size_t bytes_transferred);
   void on_read( boost::system::error_code ec
               , std::size_t bytes_transferred);
   void on_close(boost::system::error_code ec);
   void do_write();

public:
   using mgr_op_type = typename Mgr::options_type;
   explicit
   client_session( net::io_context& ioc
                 , client_session_cf op_
                 , mgr_op_type const& m);

   ~client_session()
   {
      //std::cout << "Session in destruction." << std::endl;
   }

   void run();
   void send_msg(std::string msg);
   auto const& get_mgr() const noexcept {return mgr;}
};

template <class Mgr>
client_session<Mgr>::client_session( net::io_context& ioc
                                   , client_session_cf op_
                                   , mgr_op_type const& m)
: resolver(ioc)
, timer(ioc)
, ws(ioc)
, op(std::move(op_))
, mgr(m)
{ }

inline
void fail_tmp(boost::system::error_code ec, char const* what)
{
   std::cerr << what << ": " << ec.message() << "\n";
}

template <class Mgr>
void client_session<Mgr>::on_read( boost::system::error_code ec
                                 , std::size_t bytes_transferred)
{
   boost::ignore_unused(bytes_transferred);

   if (ec) {
      if (ec == beast::websocket::error::closed) {
         // This means the session has been closed by the server.
         if (mgr.on_closed(ec) == -1) {
            //std::cout << "Cancelling timer." << std::endl;
            timer.cancel();
            return;
         }

         buffer.consume(buffer.size());

         std::cout << "Leaving on read 2 ..." << std::endl;
         return;
      }

      if (ec == net::error::operation_aborted) {
         std::cout << "Leaving on read 3: " << mgr.get_user()
                   << std::endl;
         timer.cancel();
         return;
      }

      //std::cout << "Leaving on read 4." << std::endl;
      return;
   }

   auto const str = beast::buffers_to_string(buffer.data());
   if (std::empty(str))
      throw std::runtime_error("client_session::on_read: msg empty.");

   buffer.consume(std::size(buffer));

   // Here we are canceling the after handshake timeout. The timer
   // will however be called every time this function is called.
   // TODO: Instead of canceling implement as activity with
   // expires_after.
   timer.cancel();
   auto const r = mgr.on_read(str, this->shared_from_this());
   if (r == -1) {
      do_close();
      return;
   }

   if (r == -2) {
      ws.next_layer().shutdown(net::ip::tcp::socket::shutdown_both, ec);
      ws.next_layer().close(ec);
      return;
   }

   if (r == -3) {
      ws.next_layer().close(ec);
      return;
   }

   do_read();
}

template <class Mgr>
void client_session<Mgr>::send_msg(std::string msg)
{
   auto is_empty = std::empty(msg_queue);
   msg_queue.push(std::move(msg));

   if (is_empty)
      do_write();
}

template <class Mgr>
void client_session<Mgr>::do_write()
{
   //std::cout << "Sending: " << msg << std::endl;
   auto handler = [p = this->shared_from_this()](auto ec, auto res)
   {
      p->on_write(ec, res);
   };

   ws.async_write(net::buffer(msg_queue.front()), handler);
}

template <class Mgr>
void client_session<Mgr>::do_close()
{
   if (closing)
      return;

   closing = true;

   //std::cout << "do_close" << std::endl;
   auto handler = [p = this->shared_from_this()](auto ec)
   {
      p->on_close(ec);
   };

   ws.async_close(beast::websocket::close_code::normal, handler);
}

template <class Mgr>
void client_session<Mgr>::on_close(boost::system::error_code ec)
{
   if (ec)
      fail_tmp(ec, "close");

   //std::cout << "Connection closed gracefully" << std::endl;
}

template <class Mgr>
void
client_session<Mgr>::on_connect( boost::system::error_code ec
                               , net::ip::tcp::endpoint const&)
{
   if (ec)
      return fail_tmp(ec, "resolve");

   if (mgr.on_connect() == -1) {
      // The -1 means we are are testing if the server will timeout
      // our connection if the handshake lasts too long. So here we
      // wont proceed with the handshake 
      
      // It looks like we do not need the following code to receive
      // the event that the connection was closed by the server.

      timer.expires_after(op.handshake_timeout);

      auto handler = [p = this->shared_from_this()](auto ec)
      {
         if (ec) {
            if (ec == net::error::operation_aborted) {
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
         if (ec == net::error::eof) {
            p->timer.cancel();
            //std::cout << "Timer canceled, thanks." << std::endl;
            return;
         }

         std::cout << "Unexpected error." << std::endl;
         std::cout << "Bytes transferred: " << n << std::endl;
      };

      receive_buffer.resize(32);
      ws.next_layer().async_receive( net::buffer(receive_buffer)
                                   , 0, handler2);
      return;
   }

   auto handler = [p = this->shared_from_this()](auto ec)
   { p->on_handshake(ec); };

   // Perform the websocket handshake
   ws.async_handshake(op.host, "/", handler);
}

template <class Mgr>
void client_session<Mgr>::on_handshake(boost::system::error_code ec)
{
   //std::cout << "on_handshake" << std::endl;
   if (ec)
      throw std::runtime_error(ec.message());

   // This function must be called before we begin to write commands
   // so that we can receive the acks from the server.
   do_read();

   mgr.on_handshake(this->shared_from_this());

   // This timer is used by the login test to see if the server times
   // out the connection when we do not send any command after the
   // handshake. I will be canceled on the on_read when it completes
   // with beast::websocket::error::closed, since the server is
   // expected to gracefully close the connection.  The timer is also
   // being used to test the acknowledge of the first message sent to
   // the server.
   timer.expires_after(op.auth_timeout);

   auto handler = [p = this->shared_from_this()](auto ec)
   {
      if (ec) {
         if (ec == net::error::operation_aborted) {
            // The timer has been successfully canceled.
            //std::cout << "Timer successfully canceled." << std::endl;
            return;
         }
      }

      throw std::runtime_error("client_session<Mgr>::on_handshake: fail.");
   };

   timer.async_wait(handler);
}

template <class Mgr>
void client_session<Mgr>::do_read()
{
   auto handler = [p = this->shared_from_this()](auto ec, auto res)
   { 
      p->on_read(ec, res);
   };

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

   do_write();

   if (ec)
      fail_tmp(ec, "write");
}

template <class Mgr>
void client_session<Mgr>::on_resolve( boost::system::error_code ec
                                    , net::ip::tcp::resolver::results_type results)
{
   if (ec)
      return fail_tmp(ec, "resolve");

   auto const handler = [p = this->shared_from_this()](auto ec, auto Iterator)
   {
      p->on_connect(ec, Iterator);
   };

   net::async_connect(ws.next_layer(), results, handler);
}

template <class Mgr>
void client_session<Mgr>::run()
{
   auto handler = [p = this->shared_from_this()](auto ec, auto res)
   {
      p->on_resolve(ec, res);
   };

   // Look up the domain name
   resolver.async_resolve(op.host, op.port, handler);
}

}


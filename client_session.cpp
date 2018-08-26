#include "client_session.hpp"
#include "group.hpp"

#include <chrono>

   //timer.expires_after(op.interval);
   //auto handler = [p = shared_from_this()](auto ec)
   //{ p->create_group(); };
   //timer.async_wait(handler);

namespace websocket = boost::beast::websocket;

namespace 
{

void fail(boost::system::error_code ec, char const* what)
{
   std::cerr << what << ": " << ec.message() << "\n";
}

} // Anonymous.

void client_session::on_read( boost::system::error_code ec
                            , std::size_t bytes_transferred
                            , tcp::resolver::results_type results)
{
   try {
      boost::ignore_unused(bytes_transferred);

      if (ec) {
         if (mgr.on_fail_read(ec) == -1)
            return;

         buffer.consume(buffer.size());
         //std::cout << "Connection lost, trying to reconnect." << std::endl;

         timer.expires_after(std::chrono::seconds{1});

         auto handler = [results, p = shared_from_this()](auto ec)
         { p->async_connect(results); };

         timer.async_wait(handler);
         return;
      }

      json j;
      std::stringstream ss;
      ss << boost::beast::buffers(buffer.data());
      ss >> j;
      buffer.consume(buffer.size());
      //auto str = ss.str();
      //std::cout << "Received: " << str << std::endl;

      if (mgr.on_read(j, shared_from_this()) == -1) {
         std::cerr << "Server error. Please fix." << std::endl;
         return;
      }

   } catch (std::exception const& e) {
      std::cerr << "Server error. Please fix." << std::endl;
      std::cerr << "Error: " << e.what() << std::endl;
   }

   do_read(results);
}

client_session::client_session( boost::asio::io_context& ioc
                              , client_options op_
                              , client_mgr& m)
: resolver(ioc)
, timer(ioc)
, ws(ioc)
, work(boost::asio::make_work_guard(ioc))
, op(std::move(op_))
, mgr(m)
{ }

void client_session::write(std::string msg)
{
   //std::cout << "Sending: " << msg << std::endl;
   text = std::move(msg);

   auto handler = [p = shared_from_this()](auto ec, auto res)
   { p->on_write(ec, res); };

   ws.async_write(boost::asio::buffer(text), handler);
}

void client_session::do_close()
{
   auto handler = [p = shared_from_this()](auto ec)
   { p->on_close(ec); };

   ws.async_close(websocket::close_code::normal, handler);
}

void client_session::on_resolve( boost::system::error_code ec
                               , tcp::resolver::results_type results)
{
   if (ec)
      return fail(ec, "resolve");

   async_connect(results);
}

void client_session::async_connect(tcp::resolver::results_type results)
{
   //std::cout  << "Trying to connect." << std::endl;
   auto handler = [results, p = shared_from_this()](auto ec, auto Iterator)
   { p->on_connect(ec, results); };

   boost::asio::async_connect(ws.next_layer(), results.begin(),
      results.end(), handler);
}

void
client_session::on_connect( boost::system::error_code ec
                          , tcp::resolver::results_type results)
{
   if (ec) {
      timer.expires_after(std::chrono::seconds{1});

      auto handler = [results, p = shared_from_this()](auto ec)
      { p->async_connect(results); };

      timer.async_wait(handler);
      return;
   }

   //std::cout << "Connection stablished." << std::endl;

   auto handler = [p = shared_from_this(), results](auto ec)
   { p->on_handshake(ec, results); };

   // Perform the websocket handshake
   ws.async_handshake(op.host, "/", handler);
}

void
client_session::on_handshake( boost::system::error_code ec
                            , tcp::resolver::results_type results)
{
   //std::cout << "on_handshake" << std::endl;
   if (ec)
      return fail(ec, "handshake");

   // This function must be called before we begin to write commands
   // so that we can receive a dropped connection on the server.
   do_read(results);

   // We still have no way to use the return value here. Think of a
   // solution.
   mgr.on_handshake(shared_from_this());
}

void client_session::do_read(tcp::resolver::results_type results)
{
   auto handler = [p = shared_from_this(), results](auto ec, auto res)
   { p->on_read(ec, res, results); };

   ws.async_read(buffer, handler);
}

void client_session::on_write( boost::system::error_code ec
                             , std::size_t bytes_transferred)
{
   boost::ignore_unused(bytes_transferred);

   mgr.on_write(shared_from_this());

   if (ec)
      return fail(ec, "write");
}

void client_session::on_close(boost::system::error_code ec)
{
   if (ec)
      return fail(ec, "close");

   std::cout << "Connection closed gracefully"
             << std::endl;
   work.reset();
}

void client_session::run()
{
   auto handler = [p = shared_from_this()](auto ec, auto res)
   { p->on_resolve(ec, res); };

   // Look up the domain name
   resolver.async_resolve(op.host, op.port, handler);
}


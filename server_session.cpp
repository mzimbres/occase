#include "server_session.hpp"

namespace
{

void fail(boost::system::error_code ec, char const* what)
{
   std::cerr << what << ": " << ec.message() << "\n";
}

}

server_session::server_session( tcp::socket socket
                              , std::shared_ptr<server_mgr> sd_)
: ws(std::move(socket))
, strand(ws.get_executor())
, sd(sd_)
{ }

void server_session::run()
{
   auto handler = [p = shared_from_this()](auto ec)
   { p->on_accept(ec); };

   ws.async_accept(
      boost::asio::bind_executor(strand, handler));
}

void server_session::on_accept(boost::system::error_code ec)
{
   if (ec)
      return fail(ec, "accept");

   do_read();
}

void server_session::do_read()
{
   auto handler = [p = shared_from_this()](auto ec, auto n)
   { p->on_read(ec, n); };

   ws.async_read( buffer
                , boost::asio::bind_executor(
                     strand,
                     handler));
}

void server_session::on_read( boost::system::error_code ec
                            , std::size_t bytes_transferred)
{
   boost::ignore_unused(bytes_transferred);

   if (ec == websocket::error::closed) {
      sd->on_session_closed(user_idx);
      return;
   }

   if (ec) {
      // TODO: Should this function return a value to instruct what to
      // do next or should we always drop the connection.
      sd->on_fail_read(user_idx);
      return;
   }

   std::string tmp;
   try {
      std::stringstream ss;
      ss << boost::beast::buffers(buffer.data());
      tmp = ss.str();
      buffer.consume(std::size(buffer));
      json j;
      ss >> j;
      user_idx = sd->on_read(std::move(j), shared_from_this());
      if (user_idx == -1) {
         std::cout << "Dropping connection." << tmp << std::endl;
         return;
      }
      
      if (user_idx == -2) {
         // TODO: Set a timeout here.
         std::cout << "Waiting user sms." << tmp << std::endl;
      }

      std::cout << "Accepted: " << tmp << std::endl;
   } catch (...) {
      std::cerr << "Exception from: " << tmp << std::endl;
      return;
   }

   do_read();
}

void server_session::write(std::string msg)
{
   ws.text(ws.got_text());

   auto handler = [p = shared_from_this()](auto ec, auto n)
   { p->on_write(ec, n); };

   ws.async_write( boost::asio::buffer(std::move(msg))
                 , boost::asio::bind_executor(
                      strand,
                      handler));
}

void server_session::on_write( boost::system::error_code ec
                             , std::size_t bytes_transferred)
{
   boost::ignore_unused(bytes_transferred);

   if (ec) {
      sd->on_fail_write(user_idx);
      return;
   }

   // Clear the buffer
   buffer.consume(buffer.size());
   sd->on_write(user_idx);
}


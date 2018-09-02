#include "server_session.hpp"

#include <chrono>

#include <boost/beast/websocket/rfc6455.hpp>

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
, timer( ws.get_executor().context()
       , std::chrono::steady_clock::time_point::max())
, sd(sd_)
{
   std::cout << "__________________________" << std::endl;
}

server_session::~server_session()
{
   //std::cout << "__________________________" << std::endl;
}

void server_session::do_accept()
{
   auto handler = [p = shared_from_this()](auto ec)
   { p->on_accept(ec); };

   ws.async_accept(boost::asio::bind_executor(strand, handler));
}

void server_session::on_accept_timeout(boost::system::error_code ec)
{
   // If we introduce a state we would be able to determine to which
   // action this timeout corresponds. Is it a connection timeout, a
   // login timeout etc. 

   fail(ec, "on_accept_timeout");
   if (ec == boost::asio::error::operation_aborted) {
      // The user performed operation fast enough and we do not have
      // to take any action.
      return;
   }

   // A timeout ocurred, we have to take some action. At the moment
   // all timeouts should result in closing the connection.
   do_close();
}

void server_session::on_sms_timeout(boost::system::error_code ec)
{
   // If we introduce a state we would be able to determine to which
   // action this timeout corresponds. Is it a connection timeout, a
   // login timeout etc. 

   fail(ec, "on_sms_timeout");
   if (ec == boost::asio::error::operation_aborted) {
      // The user performed operation fast enough and we do not have
      // to take any action.
      return;
   }

   // A timeout ocurred, we have to take some action. At the moment
   // all timeouts should result in closing the connection.
   do_close();
}

void server_session::on_accept(boost::system::error_code ec)
{
   if (ec)
      return fail(ec, "accept");

   // The cancelling of this timer should happen when either
   // 1. the user Identifies himself.
   // 2. The user requests a login.
   timer.expires_after(timeouts::on_accept);

   auto const handler = [p = shared_from_this()](auto ec)
   { p->on_accept_timeout(ec); };

   timer.async_wait(boost::asio::bind_executor(strand, handler));

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

void server_session::on_close(boost::system::error_code ec)
{
   if (ec)
      return fail(ec, "close");

   std::cout << "Connection closed." << std::endl;
}

void server_session::do_close()
{
   std::cout << "server_session::do_close()" << std::endl;
   auto handler = [p = shared_from_this()](auto ec)
   { p->on_close(ec); };

   websocket::close_reason reason {};
   ws.async_close(reason, handler);
}

void server_session::do_exit()
{
   std::cout << "server_session::do_exit()" << std::endl;
   timer.cancel();

   auto handler = [p = shared_from_this()](auto ec)
   { p->on_close(ec); };

   websocket::close_reason reason {};
   ws.async_close(reason, handler);
}

void server_session::on_read( boost::system::error_code ec
                            , std::size_t bytes_transferred)
{
   boost::ignore_unused(bytes_transferred);

   if (ec == websocket::error::closed) {
      timer.cancel();
      return;
   }

   if (ec == boost::asio::error::operation_aborted) {
      timer.cancel();
      return;
   }

   if (ec) {
      // TODO: Should this function return a value to instruct what to
      // do next or should we always drop the connection.
      do_exit();
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

      auto r = sd->on_read(std::move(j), shared_from_this());
      if (r == -1) {
         // -1 means that we should unconditionally close the
         // connection. 
         do_exit();
         return;
      }

      if (r == 1) {
         // Successful login request. Our state is likely on_accept
         // which means we have a timer running that we have to cancel.
         // This is where we have to set the sms timeout.
         if (timer.expires_after(timeouts::sms) > 0) {
            auto const handler = [p = shared_from_this()](auto ec)
            { p->on_sms_timeout(ec); };

            timer.async_wait(boost::asio::bind_executor(strand, handler));
         } else {
            // If we get here, it means that there was no ongoing timer.
            // But I do not see any reason for calling a login command on
            // an stablished session, this is non-sense. The
            // appropriate behaviour here seems to be to close the
            // connection.
            do_exit();

            // We do not have to async_read since we are closing the
            // connection so we can return.
            return;
         }
      } else if (r == 2) {
         // This means the sms authentification was successfull and
         // that we have to cancel the sms timer. TODO: At this point
         // we can begin to play with websockets ping pong frames.
         timer.cancel();
      }

      std::cout << "Accepted: " << tmp << std::endl;
   } catch (...) {
      std::cerr << "Exception for: " << tmp << std::endl;
      do_exit();
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
      return;
   }

   // Clear the buffer
   buffer.consume(buffer.size());
   sd->on_write(user_idx);
}


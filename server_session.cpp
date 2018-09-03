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
   //std::cout << "__________________________" << std::endl;
}

server_session::~server_session()
{
   if (login_idx != -1) {
      //std::cout << "Releasing login index." << std::endl;
      sd->release_login(login_idx);
      login_idx = -1;
   }
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
   fail(ec, "on_accept_timeout");

   // This also works
   //if (ec == boost::asio::error::operation_aborted)
   if (timer.expiry() > std::chrono::steady_clock::now()) {
      // The deadline has move, this happens only if the user send any
      // of the authentification commands. We have nothing to do.
      return;
   }

   // This deadline is not canceled. A moved deadline is caught in the
   // condition above.
   assert(!ec);

   // The timeout was triggered. That means the user did not sent us a
   // login or an authenticate command. We can either close the
   // connection gracefully by sending a close frame with async_close
   // or simply shutdown the socket, in which case the client wont be
   // notified. Here we decide to inform the user.
   do_close();
}

void server_session::on_sms_timeout(boost::system::error_code ec)
{
   fail(ec, "on_sms_timeout");
   if (ec == boost::asio::error::operation_aborted) {
      // This means the timer will not be triggered. That can happen
      // in two situations
      //
      // 1. The timer was canceled, in which case we have to release
      //    the user index to the server_mgr.
      // 2. expire_after was called to set another timer.
      //
      // We do not release the user index here and let it be handled
      // where the timer was canceled.
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
   // 1. The user Identifies himself.
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
   // TODO: Set a timeout for the received close.
}

void server_session::do_close()
{
   std::cout << "server_session::do_close()" << std::endl;
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
      // The connection has been gracefully closed. The only possible
      // pending operations now are the timers.
      timer.cancel();

      // We could also shutdown and close the socket but this is
      // redundant since the socket destructor will be called anyway
      // when we release the last reference upon return.

      // Closing the socket cancels all outstanding
      // operations. They will complete with
      // boost::asio::error::operation_aborted
      // We do not have to check the error code returned since these
      // functions close the file descriptor even on failure.
      //ws.next_layer().shutdown(tcp::socket::shutdown_both, ec);
      //ws.next_layer().close(ec);
      std::cout << "server_session::on_read: socket closed gracefully."
                << std::endl;
      return;
   }

   if (ec == boost::asio::error::operation_aborted) {
      // An aborting can be caused by a timer that has expired and
      // triggered a do_close. For example if the sms confirmation was
      // not fast enough. For precaution I will calcel any pending
      // timer and release a reference to the session by returning.
      timer.cancel();
      return;
   }

   if (ec) {
      // I still do not know what other error should be handled here.
      // I will take a default action, cance the timers and return
      // releasing one session reference.
      std::cout << "Stoping the read operation." << std::endl;
      timer.cancel();
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
         timer.cancel();
         do_close();
         return;
      }

      if (r == 1) {
         // Successful login request which means the unknown
         // connection timer  has to be canceled.  This is where we
         // have to set the sms timeout.
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
            timer.cancel();
            do_close();

            // We do not have to async_read since we are closing the
            // connection, just return and release the session
            // reference.
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
      timer.cancel();
      do_close();
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

   // TODO: The error handling here should be similar to the on_read.
   if (ec) {
      return;
   }

   // Clear the buffer
   buffer.consume(buffer.size());
   sd->on_write(user_idx);
}


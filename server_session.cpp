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
      //std::cout << "Releasing login index" << std::endl;
      sd->release_login(login_idx);
      login_idx = -1;
   }
}

void server_session::do_accept()
{
   auto handler = [p = shared_from_this()](auto ec)
   { p->on_accept(ec); };

   ws.async_accept(boost::asio::bind_executor(strand, handler));
}

void server_session::on_accept_timeout(boost::system::error_code ec)
{
   if (!ec) {
      // The timeout was triggered. That means the user did not sent us a
      // login or an authenticate command. We can either close the
      // connection gracefully by sending a close frame with async_close
      // or simply shutdown the socket, in which case the client wont be
      // notified. Here we decide to inform the user.
      do_close();
      return;
   }

   if (ec != boost::asio::error::operation_aborted) {
      // Some error, do we need to handle this?
      fail(ec, "on_accept_timeout");
      return;
   }

   // At this point we know ec = boost::asio::error::operation_aborted
   // but we do not know what caused it. Was the timer canceled or the
   // deadline has moved?
   if (timer.expiry() > std::chrono::steady_clock::now()) {
      // The deadline has move, this happens only if the user send any
      // of the authentification commands. We have nothing to do.
      return;
   }

   // The timer has been cancelled. We let the inititator handle it.
}

void server_session::on_sms_timeout(boost::system::error_code ec)
{
   // The user entry will be released in the destructor, since this is
   // the (exception) safest to do it.
   if (!ec) {
      // The timer expired. We have to release the user entry
      // allocated on the login and close the session. 
      do_close();
      return;
   }

   if (ec != boost::asio::error::operation_aborted) {
      // Some kind of error, I do not know how to handle this. I will
      // anyway release the user handler and close the session.
      do_close();
      return;
   }

   // At this point we know that ec == boost::asio::error::operation_aborted
   // which means the timer has been canceled or the deadline has
   // moved.
   if (timer.expiry() > std::chrono::steady_clock::now()) {
      // The deadline has move, this happens only if the user sends
      // the correct sms. We have nothing to do, the session will be
      // promoted to an authetified session.
      return;
   }

   // This timer has been canceled, we have to release the user entry.
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

   // TODO: Should we shutdown the socket here?
   //std::cout << "Connection closed." << std::endl;
   // TODO: Set a timeout for the received close.
}

void server_session::do_close()
{
   //std::cout << "server_session::do_close()" << std::endl;
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
      //std::cout << "server_session::on_read: socket closed gracefully."
      //          << std::endl;
      return;
   }

   if (ec == boost::asio::error::operation_aborted) {
      // Abortion can be caused by a socket shutting down and closing.
      // We have no cleanup to perform.
      return;
   }

   if (ec)
      return;

   std::string tmp;
   try {
      auto const str = boost::beast::buffers_to_string(buffer.data());
      buffer.consume(std::size(buffer));
      json j = json::parse(str);

      auto r = sd->on_read(std::move(j), shared_from_this());
      if (r == -1) {
         // We have to unconditionally close the connection. 
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
            // an stablished session, this is a logic error.
            assert(true);
         }
      } else if (r == 2) {
         // This means the sms authentification was successfull and
         // that we have to cancel the sms timer. TODO: At this point
         // we can begin to play with websockets ping pong frames.
         timer.cancel();
      } else if (r == 3) {
         // Successful authentication. We have to cancel the on accept
         // timeout.
         timer.cancel();
      }

      //std::cout << "Accepted: " << tmp << std::endl;
   } catch (...) {
      std::cerr << "Exception for: " << tmp << std::endl;
      timer.cancel();
      do_close();
      return;
   }

   do_read();
}

void server_session::on_write( boost::system::error_code ec
                             , std::size_t bytes_transferred)
{
   boost::ignore_unused(bytes_transferred);

   if (ec) {
      // The write operation failed for some reason. That means the
      // last element in the queue could not be written. This is
      // probably due to a timeout or a connection close. I will think
      // more what to do here.
      return;
   }

   buffer.consume(buffer.size()); // Clear the buffer

   msg_queue.pop();

   // TODO: Think if we will ever need this.
   sd->on_write(user_idx);

   if (msg_queue.empty())
      return; // No more message to send to the client.

   do_write(msg_queue.front());
}

void server_session::do_write(std::string msg)
{
   ws.text(ws.got_text());

   auto handler = [p = shared_from_this()](auto ec, auto n)
   { p->on_write(ec, n); };

   ws.async_write( boost::asio::buffer(std::move(msg))
                 , boost::asio::bind_executor(
                      strand,
                      handler));
}

void server_session::send_msg(std::string msg)
{
   const auto is_empty = msg_queue.empty();

   // TODO: Impose a limit on how grow the queue can grow, perhaps by
   // throwing away old messages.
   msg_queue.push(std::move(msg));

   if (is_empty)
      do_write(msg_queue.front());
}


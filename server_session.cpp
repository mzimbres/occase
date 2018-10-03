#include "server_session.hpp"

#include <chrono>

#include <boost/beast/websocket/stream.hpp>
#include <boost/beast/websocket/rfc6455.hpp>

namespace
{

void fail(boost::system::error_code ec, char const* what)
{
   std::cerr << what << ": " << ec.message() << "\n";
}

}

server_session::server_session( tcp::socket socket
                              , std::shared_ptr<server_mgr> sd_
                              , server_session_config cf_)
: ws(std::move(socket))
, strand(ws.get_executor())
, timer( ws.get_executor().context()
       , std::chrono::steady_clock::time_point::max())
, cf(cf_)
, sd(sd_)
{ }

server_session::~server_session()
{
   sd->release_user(user_id);
}

void server_session::do_accept()
{
   //auto const handler0 = [](auto kind, auto payload)
   auto const handler0 = [this](auto kind, auto payload)
   {
      if (kind == boost::beast::websocket::frame_type::close) {
         //std::cout << "Close frame received." << std::endl;
      } else if (kind == boost::beast::websocket::frame_type::ping) {
         //std::cout << "Ping frame received." << std::endl;
      } else if (kind == boost::beast::websocket::frame_type::pong) {
         std::cout << "Pong frame received." << std::endl;
         pp_state = ping_pong::pong_received;
      }

      boost::ignore_unused(kind, payload);
   };

   ws.control_callback(handler0);

   timer.expires_after(cf.handshake_timeout);

   auto const handler1 = [p = shared_from_this()](auto ec)
   {
      if (ec) {
         if (ec == boost::asio::error::operation_aborted)
            return;

         fail(ec, "do_accept_timer");
         return;
      }

      assert(!p->ws.is_open());

      p->ws.next_layer().shutdown(tcp::socket::shutdown_both, ec);
      p->ws.next_layer().close(ec);
   };

   timer.async_wait(boost::asio::bind_executor(strand, handler1));

   auto handler2 = [p = shared_from_this()](auto ec)
   {
      p->on_accept(ec);
   };

   ws.async_accept(boost::asio::bind_executor(strand, handler2));
}

void server_session::on_accept(boost::system::error_code ec)
{
   if (ec) {
      if (ec == boost::asio::error::operation_aborted) {
         // The handshake laste too long and the timer fired. giving
         // up.
         //std::cout << "Giving up on handshake." << std::endl;
         return;
      }

      fail(ec, "accept");
      return;
   }

   // The cancelling of this timer should happen when either
   // 1. The user Identifies himself.
   // 2. The user requests a login.
   timer.expires_after(cf.on_acc_timeout);

   auto const handler = [p = shared_from_this()](auto ec)
   {
      if (ec) {
         if (ec == boost::asio::error::operation_aborted)
            return;

         fail(ec, "after_handshake_timer");
         return;
      }

      p->do_close();
   };

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
   if (ec) {
      if (ec == boost::asio::error::operation_aborted) {
         // This can be caused for example if the client shuts down
         // the socket before receiving (and replying) the close frame
         //std::cout << "server_session::on_close: aborted." << std::endl;
         return;
      }

      // TODO: What should be done here?
      fail(ec, "close");
      return;
   }

   // TODO: Should we shutdown the socket here?
   //std::cout << "Connection closed." << std::endl;
   // TODO: Set a timeout for the received close.
}

void server_session::do_close()
{
    if (closing)
       return;

    closing = true;

   //std::cout << "server_session::do_close()" << std::endl;
   auto handler = [p = shared_from_this()](auto ec)
   { p->on_close(ec); };

   websocket::close_reason reason {};
   ws.async_close(reason, handler);
}

void server_session::do_pong_wait()
{
   timer.expires_after(cf.pong_wait_timeout);

   auto const handler = [p = shared_from_this()](auto ec)
   {
      if (ec) {
         if (ec == boost::asio::error::operation_aborted) {
            // Either the deadline has moved or the timer has been
            // canceled.
            return;
         }

         fail(ec, "pong_wait_handler.");
         return;
      }

      // The timer expired.
      if (p->pp_state == ping_pong::ping_sent) {
         // We did not receive the pong. We can shutdown and close the
         // socket.
         std::cout << "Peer unresponsive. Shuting down connection."
                   << std::endl;
         p->ws.next_layer().shutdown(tcp::socket::shutdown_both, ec);
         p->ws.next_layer().close(ec);
         return;
      }

      // The pong was received on time. We can initiate a new ping.
      p->do_ping();
   };

   timer.async_wait(handler);
}

void server_session::do_ping()
{
   auto const handler = [p = shared_from_this()](auto ec)
   {
      if (ec) {
         if (ec == boost::asio::error::operation_aborted) {
            // A closed frame has been sent or received before
            // the ping was sent. We have nothing to do except
            // perhaps for seting ping_state to an irrelevant
            // state.
            p->pp_state = ping_pong::unset;
            return;
         }

         fail(ec, "Ping handler");
         return;
      }

      // Sets the ping state to "ping sent".
      p->pp_state = ping_pong::ping_sent;

      // Inititates a wait on the pong.
      p->do_pong_wait();
   };

   ws.async_ping({}, handler);
}

void server_session::handle_ev(ev_res r)
{
   switch (r) {
      case ev_res::login_ok:
      {
         // Successful login request which means the ongoing
         // connection timer  has to be canceled.  This is where we
         // have to set the sms timeout.
         auto const n = timer.expires_after(cf.sms_timeout);

         auto const handler = [p = shared_from_this()](auto ec)
         {
            if (ec) {
               if (ec == boost::asio::error::operation_aborted)
                  return;

               fail(ec, "SMS timer"); // TODO: Check what to do here.
               return;
            }

            p->do_close();
         };

         timer.async_wait(boost::asio::bind_executor(strand, handler));

         // If we get here, it means that there was no ongoing timer.
         // But I do not see any reason for accepting a login command
         // on an stablished session, this is a logic error.
         assert(n > 0);
      }
      break;
      case ev_res::sms_confirmation_ok:
      {
         do_ping();
      }
      break;
      case ev_res::auth_ok:
      {
         do_ping();
      }
      break;
      case ev_res::login_fail:
      case ev_res::auth_fail:
      case ev_res::sms_confirmation_fail:
      {
         timer.cancel();
         do_close();
      }
      break;
      default:
      break;
   }
}

void server_session::on_read( boost::system::error_code ec
                            , std::size_t bytes_transferred)
{
   boost::ignore_unused(bytes_transferred);

   if (ec == websocket::error::closed) {
      // The connection has been gracefully closed. The only possible
      // pending operations now are the timers.
      //std::cout << "server_session::on_read: socket closed gracefully."
      //          << std::endl;
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
      return;
   }

   if (ec == boost::asio::error::operation_aborted) {
      // Abortion can be caused by a socket shutting down and closing.
      // We have no cleanup to perform.
      return;
   }

   if (ec)
      return;

   try {
      auto const msg = boost::beast::buffers_to_string(buffer.data());
      buffer.consume(std::size(buffer));
      auto const r = sd->on_read(std::move(msg), shared_from_this());
      handle_ev(r);
      do_read();
   } catch (...) {
      std::cerr << "Exception captured." << std::endl;
      timer.cancel();
      do_close();
   }
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

   buffer.consume(std::size(buffer)); // Clear the buffer

   msg_queue.pop();

   if (msg_queue.empty())
      return; // No more message to send to the client.

   // Do not move the front msg. If the write fail we will want to
   // save the message in the database or whatever.
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
   assert(!std::empty(msg));

   auto const is_empty = msg_queue.empty();

   // TODO: Impose a limit on how big the queue can grow.
   msg_queue.push(std::move(msg));

   if (is_empty && !closing)
      do_write(msg_queue.front());
}


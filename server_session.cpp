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

namespace rt
{

server_session::server_session( net::ip::tcp::socket socket
                              , server_mgr& mgr_)
: ws(std::move(socket))
, strand(ws.get_executor())
, timer( ws.get_executor().context()
       , std::chrono::steady_clock::time_point::max())
, mgr(mgr_)
{
   ++mgr.get_stats().number_of_sessions;
}

server_session::~server_session()
{
   //std::cout << "I am dying: " << user_id << std::endl;
   if (is_auth())
      mgr.release_auth_session(user_id);
   --mgr.get_stats().number_of_sessions;
}

void server_session::accept()
{
   auto const handler = [p = shared_from_this()]()
   {
      p->do_accept();
   };

   net::post(net::bind_executor(strand, handler));
}

void server_session::do_accept()
{
   //auto const handler0 = [](auto kind, auto payload)
   auto const handler0 = [this](auto kind, auto payload)
   {
      if (kind == beast::websocket::frame_type::close) {
         //std::cout << "Close frame received." << std::endl;
      } else if (kind == beast::websocket::frame_type::ping) {
         //std::cout << "Ping frame received." << std::endl;
      } else if (kind == beast::websocket::frame_type::pong) {
         //std::cout << "Pong frame received." << std::endl;
         pp_state = ping_pong::pong_received;
      }

      boost::ignore_unused(kind, payload);
   };

   ws.control_callback(handler0);

   timer.expires_after(mgr.get_timeouts().handshake);

   auto const handler1 = [p = shared_from_this()](auto ec)
   {
      if (ec) {
         if (ec == net::error::operation_aborted)
            return;

         fail(ec, "do_accept_timer");
         return;
      }

      assert(!p->ws.is_open());

      p->ws.next_layer().shutdown(net::ip::tcp::socket::shutdown_both, ec);
      p->ws.next_layer().close(ec);
   };

   timer.async_wait(net::bind_executor(strand, handler1));

   auto handler2 = [p = shared_from_this()](auto ec)
   {
      p->on_accept(ec);
   };

   ws.async_accept(net::bind_executor(strand, handler2));
}

void server_session::on_accept(boost::system::error_code ec)
{
   if (ec) {
      if (ec == net::error::operation_aborted) {
         // The handshake laste too long and the timer fired. giving
         // up.
         //std::cout << "Giving up on handshake." << std::endl;
         return;
      }

      fail(ec, "accept");
      return;
   }

   // The cancelling of this timer should happen when either
   // 1. The session is autheticated or a register is performed.
   // 2. The user requests a register.
   timer.expires_after(mgr.get_timeouts().auth);

   auto const handler = [p = shared_from_this()](auto ec)
   {
      if (ec) {
         if (ec == net::error::operation_aborted)
            return;

         fail(ec, "after_handshake_timer");
         return;
      }

      p->do_close();
   };

   timer.async_wait(net::bind_executor(strand, handler));

   do_read();
}

void server_session::do_read()
{
   auto handler = [p = shared_from_this()](auto ec, auto n)
   { p->on_read(ec, n); };

   ws.async_read(buffer, net::bind_executor(strand, handler));
}

void server_session::on_close(boost::system::error_code ec)
{
   if (ec) {
      if (ec == net::error::operation_aborted) {
         // This can be caused for example if the client shuts down
         // the socket before receiving (and replying) the close frame
         //std::cout << "server_session::on_close: aborted." << std::endl;
         return;
      }

      // May be caused by a socket that has already been close by the
      // peer.
      //fail(ec, "close");
      return;
   }

   // TODO: Set a timeout for the received close.
}

void server_session::do_close()
{
   if (closing)
      return;

   closing = true;

   // First we set the close frame timeout so that if the peer does
   // not reply with his close frame we do not wait forever. This
   // timer can only be canceled when on_read is called with closed.
   timer.expires_after(mgr.get_timeouts().close);

   auto const handler0 = [p = shared_from_this()](auto ec)
   {
      if (ec) {
         if (ec == net::error::operation_aborted)
            return;

         fail(ec, "on_close"); // TODO: Check what to do here.
         return;
      }

      //std::cout << "Giving up waiting for close frame. Shutting down socket."
      //          << std::endl;
      p->ws.next_layer().shutdown(net::ip::tcp::socket::shutdown_both, ec);
      p->ws.next_layer().close(ec);
   };

   timer.async_wait(net::bind_executor(strand, handler0));

   //std::cout << "server_session::do_close()" << std::endl;
   auto const handler = [p = shared_from_this()](auto ec)
   { 
      p->on_close(ec);
   };

   beast::websocket::close_reason reason {};
   ws.async_close(reason, net::bind_executor(strand, handler));
}

void server_session::send(std::string msg)
{
   auto const handler = [m = std::move(msg), p = shared_from_this()]()
   {
      p->do_send(m);
   };

   net::post(net::bind_executor(strand, handler));
}

void server_session::shutdown()
{
   auto const handler = [p = shared_from_this()]()
   {
      p->do_close();
   };

   net::post(net::bind_executor(strand, handler));
}

void server_session::do_pong_wait()
{
   timer.expires_after(mgr.get_timeouts().pong);

   auto const handler = [p = shared_from_this()](auto ec)
   {
      if (ec) {
         if (ec == net::error::operation_aborted) {
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
         //std::cout << "Peer unresponsive. Shuting down connection."
         //          << std::endl;
         p->ws.next_layer().shutdown(net::ip::tcp::socket::shutdown_both, ec);
         p->ws.next_layer().close(ec);
         return;
      }

      // The pong was received on time. We can initiate a new ping.
      p->do_ping();
   };

   timer.async_wait(net::bind_executor(strand, handler));
}

void server_session::do_ping()
{
   auto const handler = [p = shared_from_this()](auto ec)
   {
      if (ec) {
         if (ec == net::error::operation_aborted) {
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

   ws.async_ping({}, net::bind_executor(strand, handler));
}

void server_session::handle_ev(ev_res r)
{
   switch (r) {
      case ev_res::register_ok:
      {
         // Successful register request which means the ongoing
         // connection timer  has to be canceled.  This is where we
         // have to set the code timeout.
         auto const n = timer.expires_after(mgr.get_timeouts().code);

         auto const handler = [p = shared_from_this()](auto ec)
         {
            if (ec) {
               if (ec == net::error::operation_aborted)
                  return;

               fail(ec, "Code timer"); // TODO: Check what to do here.
               return;
            }

            p->do_close();
         };

         timer.async_wait(net::bind_executor(strand, handler));

         // If we get here, it means that there was no ongoing timer.
         // But I do not see any reason for accepting a register command
         // on an stablished session, this is a logic error.
         assert(n > 0);
      }
      break;
      case ev_res::code_confirmation_ok:
      {
         do_ping();
      }
      break;
      case ev_res::login_ok:
      {
         do_ping();
      }
      break;
      case ev_res::register_fail:
      case ev_res::login_fail:
      case ev_res::code_confirmation_fail:
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

   if (ec == beast::websocket::error::closed) {
      // The connection has been gracefully closed. The only possible
      // pending operations now are the timers.
      //std::cout << "server_session::on_read: socket closed gracefully."
      //          << std::endl;
      timer.cancel();

      // We could also shutdown and close the socket but this is
      // redundant since the socket destructor will be called anyway
      // when we release the last reference upon return.
      return;
   }

   if (ec == net::error::operation_aborted) {
      // Abortion can be caused by a socket shutting down and closing.
      // We have no cleanup to perform.
      return;
   }

   if (ec)
      return;

   try {
      auto const msg = beast::buffers_to_string(buffer.data());
      buffer.consume(std::size(buffer));
      auto const r = on_message(mgr, shared_from_this(), std::move(msg));
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

   msg_queue.pop();

   if (msg_queue.empty())
      return; // No more message to send to the client.

   // Do not move the front msg. If the write fail we will want to
   // save the message in the database or whatever.
   do_write(msg_queue.front());
}

void server_session::do_write(std::string const& msg)
{
   ws.text(ws.got_text());

   auto handler = [p = shared_from_this()](auto ec, auto n)
   { p->on_write(ec, n); };

   ws.async_write( net::buffer(msg)
                 , net::bind_executor(strand, handler));
}

void server_session::do_send(std::string msg)
{
   assert(!std::empty(msg));

   auto const is_empty = msg_queue.empty();

   // TODO: Impose a limit on how big the queue can grow.
   msg_queue.push(std::move(msg));

   if (is_empty && !closing)
      do_write(msg_queue.front());
}

}


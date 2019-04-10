#include "worker_session.hpp"

#include <chrono>

#include <boost/beast/websocket/stream.hpp>
#include <boost/beast/websocket/rfc6455.hpp>

#include <fmt/format.h>

#include "logger.hpp"
#include "worker.hpp"

namespace rt
{

worker_session::worker_session(net::ip::tcp::socket socket, worker& w)
: ws(std::move(socket))
, timer( ws.get_executor().context()
       , std::chrono::steady_clock::time_point::max())
, worker_(w)
{
   ++worker_.get_ws_stats().number_of_sessions;
}

worker_session::~worker_session()
{
   if (is_auth()) {
      // We also have to store all messages we weren't able to deliver
      // to the user, due to, for example, a disconnection. But we are
      // only interested in the persist messages.
      auto const cond = [](auto const& o)
         { return !o.persist; };

      auto const point =
         std::remove_if( std::begin(msg_queue)
                       , std::end(msg_queue)
                       , cond);

      auto const d = std::distance(std::begin(msg_queue), point);
      std::vector<std::string> msgs;
      msgs.reserve(d);

      auto const transformer = [](auto item)
         { return std::move(item.msg); };

      std::transform( std::make_move_iterator(std::begin(msg_queue))
                    , std::make_move_iterator(point)
                    , std::back_inserter(msgs)
                    , transformer);

      worker_.on_session_dtor(std::move(user_id), std::move(msgs));
   }

   --worker_.get_ws_stats().number_of_sessions;
}

void worker_session::accept()
{
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

      boost::ignore_unused(payload);
   };

   ws.control_callback(handler0);

   timer.expires_after(worker_.get_timeouts().handshake);

   auto const handler1 = [p = shared_from_this()](auto ec)
   {
      if (ec) {
         if (ec == net::error::operation_aborted)
            return;

         log( loglevel::debug
            , "worker_session::accept: {0}."
            , ec.message());

         return;
      }

      assert(!p->ws.is_open());

      p->ws.next_layer().shutdown(net::ip::tcp::socket::shutdown_both, ec);
      p->ws.next_layer().close(ec);
   };

   timer.async_wait(handler1);

   auto handler2 = [p = shared_from_this()](auto ec)
      { p->on_accept(ec); };

   ws.async_accept(handler2);
}

void worker_session::on_accept(boost::system::error_code ec)
{
   if (ec) {
      if (ec == net::error::operation_aborted) {
         // The handshake lasted too long and the timer fired.
         return;
      }

      log( loglevel::debug
         , "worker_session::on_accept1: {0}."
         , ec.message());
      return;
   }

   // The cancelling of this timer should happen when either
   // 1. The session is autheticated or a register is performed.
   // 2. The user requests a register.
   timer.expires_after(worker_.get_timeouts().auth);

   auto const handler = [p = shared_from_this()](auto const& ec)
   {
      if (ec) {
         if (ec == net::error::operation_aborted)
            return;

         log( loglevel::debug
            , "worker_session::on_accept2: {0}."
            , ec.message());

         return;
      }

      p->do_close();
   };

   timer.async_wait(handler);
   do_read();
}

void worker_session::do_read()
{
   auto handler = [p = shared_from_this()](auto ec, auto n)
      { p->on_read(ec, n); };

   ws.async_read(buffer, handler);
}

void worker_session::on_close(boost::system::error_code ec)
{
   if (ec) {
      if (ec == net::error::operation_aborted) {
         // This can be caused for example if the client shuts down
         // the socket before receiving (and replying) the close frame
         //std::cout << "worker_session::on_close: aborted." << std::endl;
         return;
      }

      // May be caused by a socket that has already been close by the
      // peer.
      //fail(ec, "close");
      return;
   }

   // TODO: Set a timeout for the received close.
}

void worker_session::do_close()
{
   if (closing)
      return;

   closing = true;

   // First we set the close frame timeout so that if the peer does
   // not reply with his close frame we do not wait forever. This
   // timer can only be canceled when on_read is called with closed.
   timer.expires_after(worker_.get_timeouts().close);

   auto const handler0 = [p = shared_from_this()](auto ec)
   {
      if (ec) {
         if (ec == net::error::operation_aborted)
            return;

         log( loglevel::debug
            , "worker_session::on_close0: {0}."
            , ec.message());

         return;
      }

      //std::cout << "Giving up waiting for close frame. Shutting down socket."
      //          << std::endl;
      p->ws.next_layer().shutdown(net::ip::tcp::socket::shutdown_both, ec);
      p->ws.next_layer().close(ec);
   };

   timer.async_wait(handler0);

   //std::cout << "worker_session::do_close()" << std::endl;
   auto const handler = [p = shared_from_this()](auto ec)
      { p->on_close(ec); };

   beast::websocket::close_reason reason {};
   ws.async_close(reason, handler);
}

void worker_session::send(std::string msg, bool persist)
{
   auto const is_empty = std::empty(msg_queue);

   msg_queue.push_back({std::move(msg), {}, persist});

   if (is_empty && !closing)
      do_write(msg_queue.front().msg);
}

void worker_session::send_menu_msg(std::shared_ptr<std::string> msg)
{
   auto const is_empty = std::empty(msg_queue);

   msg_queue.push_back({{}, msg, false});

   if (is_empty && !closing)
      do_write(*msg_queue.front().menu_msg);
}

void worker_session::shutdown()
{
   auto const handler = [p = shared_from_this()]()
      { p->do_close(); };

   net::post(handler);
}

void worker_session::do_pong_wait()
{
   timer.expires_after(worker_.get_timeouts().pong);

   auto const handler = [p = shared_from_this()](auto ec)
   {
      if (ec) {
         if (ec == net::error::operation_aborted) {
            // Either the deadline has moved or the timer has been
            // canceled.
            return;
         }

         log( loglevel::debug
            , "worker_session::do_pong_wait: {0}."
            , ec.message());

         return;
      }

      // The timer expired.
      if (p->pp_state == ping_pong::ping_sent) {
         // We did not receive the pong. We can shutdown and close the
         // socket.
         p->ws.next_layer().shutdown(net::ip::tcp::socket::shutdown_both, ec);
         p->ws.next_layer().close(ec);
         return;
      }

      // The pong was received on time. We can initiate a new ping.
      p->do_ping();
   };

   timer.async_wait(handler);
}

void worker_session::do_ping()
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

         log( loglevel::debug
            , "worker_session::do_ping: {0}."
            , ec.message());

         return;
      }

      // Sets the ping state to "ping sent".
      p->pp_state = ping_pong::ping_sent;

      // Inititates a wait on the pong.
      p->do_pong_wait();
   };

   ws.async_ping({}, handler);
}

void worker_session::handle_ev(ev_res r)
{
   switch (r) {
      case ev_res::register_ok:
      {
         // Successful register request which means the ongoing
         // connection timer  has to be canceled.  This is where we
         // have to set the code timeout.
         auto const n = timer.expires_after(worker_.get_timeouts().code);

         auto const handler = [p = shared_from_this()](auto const& ec)
         {
            if (ec) {
               if (ec == net::error::operation_aborted)
                  return;

               log( loglevel::debug, "worker_session::handle_ev: {0}."
                  , ec.message());

               return;
            }

            p->do_close();
         };

         timer.async_wait(handler);

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

void worker_session::on_read( boost::system::error_code ec
                            , std::size_t bytes_transferred)
{
   boost::ignore_unused(bytes_transferred);

   if (ec == beast::websocket::error::closed) {
      // The connection has been gracefully closed. The only possible
      // pending operations now are the timers.
      //std::cout << "worker_session::on_read: socket closed gracefully."
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
      auto const r = worker_.on_message(shared_from_this(), std::move(msg));
      handle_ev(r);
      do_read();
   } catch (std::exception const& e) {
      log( loglevel::debug
         , "worker_session::on_read: {0}."
         , e.what());

      timer.cancel();
      do_close();
   }
}

void worker_session::on_write( boost::system::error_code ec
                             , std::size_t bytes_transferred)
{
   boost::ignore_unused(bytes_transferred);

   if (ec) {
      // The write operation failed for some reason. That means the
      // last element in the queue could not be written. This is
      // probably due to a timeout or a connection close. We have
      // nothing to do. Unsent user messages will be returned to the
      // database so that the server can retrieve them next time he
      // reconnects.
      return;
   }

   msg_queue.pop_front();

   if (std::empty(msg_queue))
      return; // No more message to send to the client.

   // Do not move the front msg. If the write fail we will want to
   // save the message in the database or whatever.
   if (msg_queue.front().menu_msg) {
      do_write(*msg_queue.front().menu_msg);
   } else {
      do_write(msg_queue.front().msg);
   }
}

void worker_session::do_write(std::string const& msg)
{
   ws.text(ws.got_text());

   auto handler = [p = shared_from_this()](auto ec, auto n)
      { p->on_write(ec, n); };

   ws.async_write(net::buffer(msg), handler);
}

std::weak_ptr<proxy_session>
worker_session::get_proxy_session(bool make_new_session)
{
   if (!psession || make_new_session) {
      psession = std::make_shared<proxy_session>();
      psession->session = shared_from_this();
   }

   return psession;
}

}


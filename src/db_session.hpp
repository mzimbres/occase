#pragma once

#include <deque>
#include <chrono>
#include <memory>
#include <atomic>
#include <cstdint>

#include <boost/asio/steady_timer.hpp>
#include <boost/container/static_vector.hpp>

#include <fmt/format.h>

#include "net.hpp"
#include "logger.hpp"
#include "menu.hpp"
#include "json_utils.hpp"

namespace rt
{

template <class Derived>
class db_session;

enum class ev_res
{ register_ok
, register_fail
, login_ok
, login_fail
, subscribe_ok
, subscribe_fail
, publish_ok
, publish_fail
, chat_msg_ok
, chat_msg_fail
, delete_ok
, delete_fail
, filenames_ok
, filenames_fail
, presence_ok
, presence_fail
, unknown
};

struct ws_timeouts {
   std::chrono::seconds handshake {2};
   std::chrono::seconds idle {2};
};

template <class Derived>
class db_session {
private:
   beast::multi_buffer buffer;

   // The pong counter is used to decide when the login timeout
   // occurrs, at the moment it is hardcoded to 2.
   int pong_counter = 0;

   struct msg_entry {
      std::string msg;
      std::shared_ptr<std::string> menu_msg;
      bool persist;
   };

   std::deque<msg_entry> msg_queue;

   bool closing = false;

   std::string user_id;

   std::uint64_t any_of_features = 0;

   static constexpr auto menu_codes_size = 32;

   using menu_codes_type =
      boost::container::static_vector< std::uint64_t
                                     , menu_codes_size>;

   menu_codes_type menu_codes;

   Derived& derived() { return static_cast<Derived&>(*this); }

   void do_read()
   {
      auto self = derived().shared_from_this();
      auto handler = [self](auto ec, auto n)
         { self->on_read(ec, n); };

      derived().ws().async_read(buffer, handler);
   }

   void do_write(std::string const& msg)
   {
      derived().ws().text(derived().ws().got_text());

      auto self = derived().shared_from_this();
      auto handler = [self](auto ec, auto n)
         { self->on_write(ec, n); };

      derived().ws().async_write(net::buffer(msg), handler);
   }


   void on_read(boost::system::error_code ec, std::size_t bytes_transferred)
   {
      boost::ignore_unused(bytes_transferred);

      if (ec == beast::websocket::error::closed) {
         log( loglevel::debug
            , "db_session::on_read: Gracefully closed {0}."
            , user_id);

         return;
      }

      if (ec == net::error::operation_aborted) {
         // Abortion can be caused by a socket shutting down and closing.
         // We have no cleanup to perform.
         return;
      }

      if (ec)
         return;

      auto msg = beast::buffers_to_string(buffer.data());
      buffer.consume(std::size(buffer));
      auto self = derived().shared_from_this();
      auto const r = derived().db().on_app(self, std::move(msg));
      handle_ev(r);
      do_read();
   }

   void on_close(boost::system::error_code ec)
   {
      if (ec) {
         if (ec == net::error::operation_aborted) {
            // This can be caused for example if the client shuts down
            // the socket before receiving (and replying) the close frame
            return;
         }

         // May be caused by a socket that has already been close by the
         // peer.
         //fail(ec, "close");
         return;
      }
   }

   void on_accept(boost::system::error_code ec)
   {
      if (ec) {
         if (ec == beast::error::timeout) {
            // The handshake lasted too long and the timer fired.
            return;
         }

         auto const err = ec.message();
         auto ed = derived().ws().next_layer().socket().remote_endpoint(ec).address().to_string();
         if (ec)
            ed = ec.message();

         log( loglevel::debug
            , "db_session::on_accept1: {0}. Remote endpoint: {1}."
            , err
            , ed);

         return;
      }

      do_read();
   }

   void on_write( boost::system::error_code ec
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

   void handle_ev(ev_res r)
   {
      switch (r) {
         case ev_res::register_fail:
         case ev_res::login_fail:
         case ev_res::subscribe_fail:
         case ev_res::publish_fail:
         case ev_res::chat_msg_fail:
         case ev_res::delete_fail:
         case ev_res::filenames_fail:
         case ev_res::presence_fail:
         case ev_res::unknown:
         {
            shutdown();
         }
         break;
         default:
         break;
      }
   }

   void do_send(msg_entry entry);

public:

   // NOTE: We cannot access the bases class members here since it is
   // not constructed yet. Do not declare the constructor.

   ~db_session()
   {
      try {
         --derived().db().get_ws_stats().number_of_sessions;

         if (is_logged_in()) {
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

            derived().db().on_session_dtor(std::move(user_id), std::move(msgs));
         }
      } catch (...) {
      }
   }

   void accept()
   {
      ++derived().db().get_ws_stats().number_of_sessions;

      using timeout_type = websocket::stream_base::timeout;

      timeout_type wstm
      { derived().db().get_timeouts().handshake
      , derived().db().get_timeouts().idle
      , true
      };

      derived().ws().set_option(wstm);

      auto f = [](websocket::response_type& res)
      {
          res.set( http::field::server
                 , std::string(BOOST_BEAST_VERSION_STRING) + " occase");
      };

      derived().ws().set_option(websocket::stream_base::decorator(f));

      auto const handler0 = [this](auto kind, auto payload)
      {
         boost::ignore_unused(payload);

         if (kind == beast::websocket::frame_type::close) {
         } else if (kind == beast::websocket::frame_type::ping) {
         } else if (kind == beast::websocket::frame_type::pong) {
            if (++pong_counter > 1 && !is_logged_in())
               shutdown();
         }
      };

      derived().ws().control_callback(handler0);

      auto self = derived().shared_from_this();
      auto handler2 = [self](auto ec)
         { self->on_accept(ec); };

      derived().ws().async_accept(handler2);
   }

   // Messages for which persist is true will be persisted on the
   // database and sent to the user next time he reconnects.
   void send(std::string msg, bool persist)
   {
      assert(!std::empty(msg));
      auto const is_empty = std::empty(msg_queue);

      msg_queue.push_back({std::move(msg), {}, persist});

      if (is_empty && !closing)
         do_write(msg_queue.front().msg);
   }

   void send_post( std::shared_ptr<std::string> msg
                 , std::uint64_t hash_code
                 , std::uint64_t features)
   {
      if (any_of_features != 0) {
         if ((any_of_features & features) == 0)
            return;
      }

      if (!std::empty(menu_codes)) {
         auto const match =
            std::binary_search( std::begin(menu_codes)
                              , std::end(menu_codes)
                              , hash_code);
         if (!match)
            return;
      }

      auto const is_empty = std::empty(msg_queue);

      msg_queue.push_back({{}, msg, false});

      if (is_empty && !closing)
         do_write(*msg_queue.front().menu_msg);
   }

   void shutdown()
   {
      if (closing)
         return;

      log(loglevel::debug, "db_session::shutdown: {0}.", user_id);

      closing = true;

      auto self = derived().shared_from_this();
      auto handler = [self](auto ec)
         { self->on_close(ec); };

      beast::websocket::close_reason reason {};
      derived().ws().async_close(reason, handler);
   }

   void set_id(std::string id)
      { user_id = std::move(id); };

   // Sets the any_of filter, used to filter posts sent with send_post
   // above. If the filter is non-null the post features will be
   // required to contain at least one bit set that is also set in the
   // argument passed here.
   void set_any_of_features(std::uint64_t o)
      { any_of_features = o; }

   void set_filter(std::vector<std::uint64_t> const& codes)
   {
      auto const min = std::min(ssize(codes), menu_codes_size);
      menu_codes.clear();
      std::copy( std::cbegin(codes)
               , std::cbegin(codes) + min
               , std::back_inserter(menu_codes));
   }

   auto const& get_id() const noexcept
      { return user_id;}
   auto is_logged_in() const noexcept
      { return !std::empty(user_id);};
};

}


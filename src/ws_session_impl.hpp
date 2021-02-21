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
#include "post.hpp"
#include "logger.hpp"

namespace occase {

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
, presence_ok
, presence_fail
, unknown
};

template <class Derived>
class ws_session_impl {
private:
   static auto constexpr ranges_size_ = 2 * 3;

   struct msg_entry {
      std::string msg;
      std::shared_ptr<std::string> menu_msg;
      bool persist;
   };

   beast::multi_buffer buffer_;
   // The pong counter is used to decide when the login timeout
   // occurrs, at the moment it is hardcoded to 2.
   int pong_counter_ = 0;
   std::deque<msg_entry> msg_queue_;
   bool closing_ = false;
   std::string pub_hash_;
   code_type any_of_filter_ = 0;

   boost::container::static_vector<code_type, ranges_size_> ranges_;

   Derived& derived() { return static_cast<Derived&>(*this); }

   void do_read()
   {
      auto self = derived().shared_from_this();
      auto handler = [self](auto ec, auto n)
         { self->on_read(ec, n); };

      derived().ws().async_read(buffer_, handler);
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

      if (ec) {
         finish();
         log::write(log::level::debug,
	            "ws_session_impl::on_read: {0}. User {1}",
		    ec.message(),
		    pub_hash_);
         return;
      }

      auto msg = beast::buffers_to_string(buffer_.data());
      buffer_.consume(std::size(buffer_));
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

   void on_run(boost::system::error_code ec)
   {
      if (ec) {
         if (ec == beast::error::timeout) {
            // The handshake lasted too long and the timer fired.
            return;
         }

         //auto const err = ec.message();
         //auto ed = derived().ws().next_layer().socket().remote_endpoint(ec).address().to_string();
         //if (ec)
         //   ed = ec.message();

         //log( log::level::debug
         //   , "ws_session_impl::on_accept1: {0}. Remote endpoint: {1}."
         //   , err
         //   , ed);

         return;
      }

      ++derived().db().get_ws_stats().number_of_sessions;
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

      msg_queue_.pop_front();

      if (std::empty(msg_queue_))
         return; // No more message to send to the client.

      // Do not move the front msg. If the write fail we will want to
      // save the message in the database or whatever.
      if (msg_queue_.front().menu_msg) {
         do_write(*msg_queue_.front().menu_msg);
      } else {
         do_write(msg_queue_.front().msg);
      }
   }

   void handle_ev(ev_res r)
   {
      switch (r) {
         case ev_res::register_fail:
         case ev_res::login_fail:
         case ev_res::subscribe_fail:
         case ev_res::chat_msg_fail:
         case ev_res::unknown:
         {
            shutdown();
         }
         break;
         default:
         break;
      }
   }

   void finish()
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
               std::remove_if( std::begin(msg_queue_)
                             , std::end(msg_queue_)
                             , cond);

            auto const d = std::distance(std::begin(msg_queue_), point);
            std::vector<std::string> msgs;
            msgs.reserve(d);

            auto const transformer = [](auto item)
               { return std::move(item.msg); };

            std::transform( std::make_move_iterator(std::begin(msg_queue_))
                          , std::make_move_iterator(point)
                          , std::back_inserter(msgs)
                          , transformer);

            derived().db().on_session_dtor(pub_hash_, msgs);
         }
      } catch (...) {
      }
   }


public:
   // NOTE: We cannot access the bases class members here since it is
   // not constructed yet.

   template <class Body, class Allocator>
   void run(http::request<Body, http::basic_fields<Allocator>> req)
   {
      using timeout_type = websocket::stream_base::timeout;

      timeout_type wstm
      { derived().db().get_timeouts().handshake
      , derived().db().get_timeouts().idle
      , true
      };

      derived().ws().set_option(wstm);
      auto const name = derived().db().get_cfg().server_name;

      auto f = [=](websocket::response_type& res)
         { res.set(http::field::server, name); };

      derived().ws().set_option(websocket::stream_base::decorator(f));

      auto const handler0 = [this](auto kind, auto payload)
      {
         boost::ignore_unused(payload);

         if (kind == beast::websocket::frame_type::close) {
         } else if (kind == beast::websocket::frame_type::ping) {
         } else if (kind == beast::websocket::frame_type::pong) {
            if (++pong_counter_ > 1 && !is_logged_in())
               shutdown();
         }
      };

      derived().ws().control_callback(handler0);

      auto self = derived().shared_from_this();
      auto handler2 = [self](auto ec)
         { self->on_run(ec); };

      derived().ws().async_accept(req, handler2);
   }

   // Messages for which persist is true will be persisted on the
   // database and sent to the user next time he reconnects.
   void send(std::string msg, bool persist)
   {
      assert(!std::empty(msg));
      auto const is_empty = std::empty(msg_queue_);

      msg_queue_.push_back({std::move(msg), {}, persist});

      if (is_empty && !closing_)
         do_write(msg_queue_.front().msg);
   }

   auto ignore(post const& p) const noexcept
   {
      if (!std::empty(ranges_)) {
         auto const min =
            std::min(std::ssize(ranges_) / 2,
                     std::ssize(p.range_values));

         for (auto i = 0; i < min; ++i) {
            auto const a = ranges_[2 * i + 0];
            auto const b = ranges_[2 * i + 1];
            auto const v = p.range_values[i];
            if (v < a || b < v)
               return true; // Not in range.
         }
      }

      return false;
   }

   void send_post(std::shared_ptr<std::string> msg, post const& p)
   {
      if (ignore(p))
         return;

      auto const is_empty = std::empty(msg_queue_);

      msg_queue_.push_back({{}, msg, false});

      if (is_empty && !closing_)
         do_write(*msg_queue_.front().menu_msg);
   }

   void shutdown()
   {
      if (closing_)
         return;

      log::write(log::level::debug, "ws_session_impl::shutdown: {0}.", pub_hash_);

      closing_ = true;

      auto self = derived().shared_from_this();
      auto handler = [self](auto ec)
         { self->on_close(ec); };

      beast::websocket::close_reason reason {};
      derived().ws().async_close(reason, handler);
   }

   void set_pub_hash(std::string hash) { pub_hash_ = std::move(hash); };
   auto const& get_pub_hash() const noexcept { return pub_hash_;}
   auto is_logged_in() const noexcept { return !std::empty(pub_hash_);};
};

}


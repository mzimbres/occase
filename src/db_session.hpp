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
#include "utils.hpp"

namespace occase
{

template <class Session>
struct proxy_session {
   // We have to use a weak pointer here. A share_ptr doen't work.
   // Even when the proxy_session object is explicitly killed. The
   // object is itself killed only after the last weak_ptr is
   // destructed. The shared_ptr will live that long in that case.
   std::weak_ptr<Session> session;
};

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
   std::chrono::seconds handshake;
   std::chrono::seconds idle;
};

template <class Derived>
class db_session {
private:
   static auto constexpr sub_channels_size = 64;
   static auto constexpr ranges_size = 5;

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
   std::string user_id_;
   code_type any_of_filter_ = 0;

   boost::container::static_vector<
      code_type, sub_channels_size> sub_channels_;

   boost::container::static_vector<
      code_type, ranges_size> ranges_;

   // The number of posts the user can publish until the deadline.
   int remaining_posts_ = 0;

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
         log::write( log::level::debug
                   , "db_session::on_read: {0}. User {1}"
                   , ec.message(), user_id_);
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
         //   , "db_session::on_accept1: {0}. Remote endpoint: {1}."
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
         case ev_res::delete_fail:
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

            derived().db().on_session_dtor(std::move(user_id_), std::move(msgs));
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
      if (any_of_filter_ != 0) {
         if ((any_of_filter_ & p.features) == 0)
            return true;
      }

      if (!std::empty(sub_channels_)) {
         auto const match =
            std::binary_search( std::begin(sub_channels_)
                              , std::end(sub_channels_)
                              , p.filter);
         if (!match)
            return true;
      }

      if (!std::empty(ranges_)) {
         auto const min =
            std::min( ssize(ranges_) / 2
                    , ssize(p.range_values));

         for (auto i = 0; i < min; ++i) {
            auto const a = ranges_[2 * i];
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

      log::write(log::level::debug, "db_session::shutdown: {0}.", user_id_);

      closing_ = true;

      auto self = derived().shared_from_this();
      auto handler = [self](auto ec)
         { self->on_close(ec); };

      beast::websocket::close_reason reason {};
      derived().ws().async_close(reason, handler);
   }

   void set_id(std::string id)
      { user_id_ = std::move(id); };

   // Sets the any_of filter, used to filter posts sent with send_post
   // above. If the filter is non-null the post features will be
   // required to contain at least one bit set that is also set in the
   // argument passed here.
   void set_any_of_filter(code_type o)
      { any_of_filter_ = o; }

   void set_sub_channels(std::vector<code_type> const& codes)
   {
      auto const min = std::min(ssize(codes), sub_channels_size);
      sub_channels_.clear();
      std::copy( std::cbegin(codes)
               , std::cbegin(codes) + min
               , std::back_inserter(sub_channels_));
   }

   void set_ranges(std::vector<int> const& ranges)
   {
      auto const min = std::min(ssize(ranges), ranges_size);
      ranges_.clear();
      std::copy( std::cbegin(ranges)
               , std::cbegin(ranges) + min
               , std::back_inserter(ranges_));
   }

   auto const& get_id() const noexcept
      { return user_id_;}

   auto is_logged_in() const noexcept
      { return !std::empty(user_id_);};

   void set_remaining_posts(int n) noexcept
      { remaining_posts_ = n; }

   auto get_remaining_posts() const noexcept
      { return remaining_posts_; }

   auto decrease_remaining_posts() noexcept
      { return --remaining_posts_; }
};

}


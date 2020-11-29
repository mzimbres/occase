#pragma once

#include <vector>
#include <string>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <boost/asio.hpp>
#include <aedis/aedis.hpp>

#include <fmt/format.h>
#include <fmt/ostream.h>

#include "net.hpp"
#include "logger.hpp"

namespace occase {

struct instance {
   std::string host;
   std::string port;
   std::string name;
};

struct instance_op {
   enum class states
   { starting
   , writing
   , waiting };

   tcp::socket& socket_;
   aedis::resp::buffer* buffer_;
   instance* instance_;
   states state_ {states::starting};
   std::string cmd_;

   template <class Self>
   void operator()( Self& self
                  , boost::system::error_code ec = {}
                  , std::size_t n = 0)
   {
      switch (state_) {
      case states::starting:
      {
         cmd_ = aedis::sentinel("get-master-addr-by-name", instance_->name);
         state_ = states::writing;
         net::async_write( socket_
                         , net::buffer(cmd_)
                         , std::move(self));
      } break;
      case states::writing:
      {
         if (ec)
            return self.complete(ec);

         state_ = states::waiting;
	 aedis::resp::async_read(socket_, buffer_, std::move(self));

      } break;
      case states::waiting:
      {
         auto n = std::size(buffer_->res);
         if (n > 1) {
            instance_->host = buffer_->res[0];
            instance_->port = buffer_->res[1];
         }
         self.complete(ec);
      } break;
      default:
      {
      }
      }
   }
};

template <class CompletionToken>
auto
async_get_instance( tcp::socket& s
                  , aedis::resp::buffer* p
                  , instance* p2
                  , CompletionToken&& token)
{
   return net::async_compose
      < CompletionToken
      , void(boost::system::error_code)
      >(instance_op {s, p, p2}, token, s);
}

class redis_session {
public:
   using on_conn_handler_type = std::function<void()>;

   using msg_handler_type =
      std::function<void( boost::system::error_code const&
                        , std::vector<std::string>)>;

  struct config {
     // A list of redis sentinels e.g. ip1 port1 ip2 port2 ...
     std::vector<std::string> sentinels {"127.0.0.1", "26379"};
     std::string name {"mymaster"};
     std::string role {"master"};
     int max_pipeline_size {256};
     log::level log_filter {log::level::debug};
  };

private:
   struct queue_item {
      std::string payload;
      bool sent;
   };

   std::string id_;
   config cfg_;
   ip::tcp::resolver resolver_;
   tcp::socket socket_;

   net::steady_timer timer_;
   aedis::resp::buffer buffer_;
   std::queue<queue_item> msg_queue_;
   int pipeline_size_ = 0;
   long long pipeline_id_ = 0;
   instance instance_;
   int sentinel_idx_ = 0;
   bool disable_reconnect_ = false;

   msg_handler_type msg_handler_ = [this](auto ec, auto const& res)
   {
      if (ec) {
         log::write( log::level::debug
                   , "{0}/msg_handler: {1}."
                   , id_
                   , ec.message());
      }

      std::copy( std::cbegin(res)
               , std::cend(res)
               , std::ostream_iterator<std::string>(std::cout, " "));

      std::cout << std::endl;
   };


   on_conn_handler_type conn_handler_ = [](){};

   void do_read_resp()
   {
      buffer_.res.clear();

      auto f = [this](auto const& ec)
         { on_resp(ec); };

      aedis::resp::async_read(socket_, &buffer_, std::move(f));
   }

   void
   on_instance( std::string const& host
              , std::string const& port
              , boost::system::error_code ec)
   {
      buffer_.clear();

      if (ec) {
         log::write( log::level::warning
                   , "{0}/on_instance: {1}. Endpoint: {2}"
                   , id_
                   , ec.message());
         return;
      }

      // Closes the connection with the sentinel and connects with the
      // master.
      close("{0}/on_instance: {1}.");

      // NOTE: Call sync resolve to prevent asio from starting a new
      // thread.
      ec = {};
      auto res = resolver_.resolve(host, port, ec);
      if (ec) {
         log::write( log::level::warning
                   , "{0}/on_instance: {1}."
                   , id_
                   , ec.message());
         return;
      }

      do_connect(res);
   }

   void do_connect(net::ip::tcp::resolver::results_type res)
   {
      auto f = [this](auto ec, auto iter)
         { on_connect(ec, iter); };

      net::async_connect(socket_, res, f);
   }

   void on_connect( boost::system::error_code ec
                  , net::ip::tcp::endpoint const& endpoint)
   {
      if (ec) {
         log::write( log::level::warning
                   , "{0}/on_connect: {1}. Endpoint: {2}"
                   , id_
                   , ec.message()
                   , endpoint);

         if (!disable_reconnect_)
            run();

         return;
      }

      log::write( log::level::info
                , "{0}/Success connecting to redis instance {1}"
                , id_
                , endpoint);

      do_read_resp();

      // Consumes any messages that have been eventually posted while the
      // connection was not established, or consumes msgs when a
      // connection to redis is restablished.
      if (!std::empty(msg_queue_)) {
         log::write( log::level::debug
                   , "{0}/on_connect: Number of messages {1}"
                   , id_
                   , std::size(msg_queue_));
         do_write();
      }

      // Calls user callback to inform a successfull connect to redis.
      // It may wish to start sending some commands.
      //
      // Since this callback may call the send member function on this
      // object, we have to call it AFTER the write operation above,
      // otherwise the message will be sent twice.
      conn_handler_();
   }

   void on_resp(boost::system::error_code ec)
   {
      if (ec) {
         log::write( log::level::warning
                   , "{0}/on_resp: {1}."
                   , id_
                   , ec.message());

         // Some of the possible errors are.
         // net::error::eof
         // net::error::connection_reset
         // net::error::operation_aborted

         close("{0}/on_resp: {1}.");
         if (!disable_reconnect_)
            run();

         return;
      }

      msg_handler_(ec, std::move(buffer_.res));

      do_read_resp();

      if (std::empty(msg_queue_))
         return;

      // In practive, the if condition below will always hold as we pop the
      // last written message as soon as the first response from a pipeline
      // is received and send the next. If think the code is clearer this
      // way.
      if (msg_queue_.front().sent) {
         msg_queue_.pop();

         if (std::empty(msg_queue_))
            return;

         do_write();
      }
   }

   void on_write(boost::system::error_code ec, std::size_t n)
   {
      if (ec) {
         log::write( log::level::info
                   , "{0}/on_write: {1}."
                   , id_, ec.message());
         return;
      }
   }

   void do_write()
   {
      auto f = [this](auto ec, auto n)
         { on_write(ec, n); };

      assert(!std::empty(msg_queue_));
      assert(!std::empty(msg_queue_.front().payload));

      net::async_write( socket_
                      , net::buffer(msg_queue_.front().payload)
                      , f);
      msg_queue_.front().sent = true;
   }

   void close(char const* msg)
   {
      boost::system::error_code ec;
      socket_.close(ec);
      if (ec) {
         log::write( log::level::warning
                   , msg
                   , id_
                   , ec.message());
      }
   }

   void
   on_sentinel_conn( boost::system::error_code ec
                   , net::ip::tcp::endpoint const& endpoint)
   {
      if (ec) {
         log::write( log::level::warning
                   , "{0}/on_sentinel_conn: {1}. Endpoint: {2}"
                   , id_
                   , ec.message()
                   , endpoint);

         // Ask the next sentinel only if we did not try them all yet.
         ++sentinel_idx_;
         if ((2 * sentinel_idx_) == std::size(cfg_.sentinels)) {
            log::write( log::level::warning
                      , "{0}/No sentinel knows the redis instance address."
                      , id_);

            return;
         }

         run(); // Tries the next sentinel.
         return;
      }

      // The redis documentation recommends to put the first sentinel that
      // replies in the start of the list. See
      // https://redis.io/topics/sentinel-clients
      if (sentinel_idx_ != 0) {
         auto const r = sentinel_idx_;
         std::swap(cfg_.sentinels[0], cfg_.sentinels[2 * r]);
         std::swap(cfg_.sentinels[1], cfg_.sentinels[2 * r + 1]);
         sentinel_idx_ = 0;
      }

      log::write( log::level::info
                , "{0}/Success connecting to sentinel {1}"
                , id_
                , endpoint);

      auto g = [this](auto ec)
         { on_instance(instance_.host, instance_.port, ec); };

      instance_.name = cfg_.name;
      async_get_instance(socket_, &buffer_, &instance_, std::move(g));
   }

public:
   redis_session( net::io_context& ioc
          , config cfg
          , std::string id = "aedis")
   : id_(id)
   , cfg_ {std::move(cfg)}
   , resolver_ {ioc} 
   , socket_ {ioc}
   , timer_ {ioc, std::chrono::steady_clock::time_point::max()}
   {
      if (cfg_.max_pipeline_size < 1)
         cfg_.max_pipeline_size = 1;
   }

   redis_session(net::io_context& ioc) : redis_session {ioc, {}, {}} { }

   void set_on_conn_handler(on_conn_handler_type f)
      { conn_handler_ = std::move(f);};

   void set_msg_handler(msg_handler_type f)
      { msg_handler_ = std::move(f);};

   auto send(std::string msg)
   {
      assert(!std::empty(msg));

      auto const max_pp_size_reached =
         pipeline_size_ >= cfg_.max_pipeline_size;

      if (max_pp_size_reached)
         pipeline_size_ = 0;

      auto const is_empty = std::empty(msg_queue_);

      // When std::size(msg_queue_) == 1 we know the message in the back of
      // queue has already been sent and we are waiting for a reponse, we
      // cannot pipeline in this case.
      if (is_empty || std::size(msg_queue_) == 1 || max_pp_size_reached) {
         msg_queue_.push({std::move(msg), false});
	 ++pipeline_id_;
      } else {
         msg_queue_.back().payload += msg; // Uses pipeline.
         ++pipeline_size_;
      }

      if (is_empty && socket_.is_open())
         do_write();

      return pipeline_id_;
   }

   void run()
   {
      auto const n = std::size(cfg_.sentinels);

      if (n == 0 || (n % 2 != 0)) {
         log::write( log::level::warning
                   , "{0}/run: Incompatible sentinels array size: {1}"
                   , id_, n);
         return;
      }

      auto const r = sentinel_idx_;

      boost::system::error_code ec;
      auto res = resolver_
         .resolve( cfg_.sentinels[2 * r]
                 , cfg_.sentinels[2 * r + 1]
                 , ec);

      if (ec) {
         log::write( log::level::warning
                   , "{0}/run: Can't resolve sentinel: {1}."
                   , id_
                   , ec.message());
         return;
      }

      auto f = [this](auto ec, auto iter)
      { on_sentinel_conn(ec, iter); };

      net::async_connect(socket_, res, f);
   }

   void disable_reconnect()
   {
      assert(!disable_reconnect_);
      disable_reconnect_ = true;
   }
};

}

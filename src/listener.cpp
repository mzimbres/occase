#include "listener.hpp"

#include <iostream>
#include <algorithm>

#include <fmt/format.h>
#include <fmt/ostream.h>

#include "logger.hpp"
#include "worker.hpp"
#include "worker_session.hpp"

namespace rt
{

struct arena {
   net::io_context ioc {1};
   worker worker_;
   std::thread thread_;

   arena(worker_cfg const& cfg, int i)
   : worker_ {cfg, i, ioc}
   , thread_ { std::thread {[this](){ run();}} }
   {}

   void run() noexcept
   {
      try {
         ioc.run();
      } catch (std::exception const& e) {
         std::cout << e.what() << std::endl;
      }
   }
};

listener::listener(listener_cfg const& cfg)
: signals {ioc, SIGINT, SIGTERM}
, acceptor {ioc, {boost::asio::ip::tcp::v4(), cfg.port}}
{
   log( loglevel::info
      , "Binding server to {}"
      , acceptor.local_endpoint());

   auto arena_gen = [&cfg, i = -1]() mutable
      { return std::make_shared<arena>(cfg.worker, ++i); };

   std::generate_n( std::back_inserter(arenas)
                  , cfg.number_of_workers
                  , arena_gen);
}

void listener::shutdown()
{
   acceptor.cancel();

   auto joiner = [](auto& o)
      { o->thread_.join(); };

   std::for_each(std::begin(arenas), std::end(arenas), joiner);
}

void listener::run()
{
   auto const handler = [this](auto ec, auto n)
   {
      // TODO: Verify ec here.
      log(loglevel::info, "Beginning the shutdown operation.");
      shutdown();
   };

   signals.async_wait(handler);

   if (!acceptor.is_open())
      return;

   do_accept();

   ioc.run();
}

void listener::do_accept()
{
   auto handler = [this](auto const& ec, auto socket)
      { on_accept(ec, std::move(socket)); };

   auto const n = next % std::size(arenas);
   acceptor.async_accept(arenas[n]->ioc, handler);
}

void listener::on_accept( boost::system::error_code ec
                        , net::ip::tcp::socket peer)
{
   if (ec) {
      if (ec == net::error::operation_aborted) {
         log(loglevel::info, "Stopping accepting connections");
         return;
      }

      log( loglevel::debug, "listener::on_accept: {0}", ec.message());
   } else {
      auto const n = next % std::size(arenas);
      std::make_shared< worker_session
                      >(std::move(peer), arenas[n]->worker_)->accept();
      ++next;
   }

   do_accept();
}

}


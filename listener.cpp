#include "listener.hpp"

#include <iostream>
#include <algorithm>

#include <fmt/format.h>
#include <fmt/ostream.h>

#include "server_session.hpp"
#include "worker.hpp"

namespace rt
{

struct arena {
   net::io_context ioc {1};
   worker worker_;
   std::thread thread_;

   arena(server_cf const& cfg, int i)
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
   auto const* fmt1 = "Binding server to {}";
   log( fmt::format(fmt1, acceptor.local_endpoint())
      , loglevel::info);

   auto arena_gen = [&cfg, i = -1]() mutable
      { return std::make_shared<arena>(cfg.mgr, ++i); };

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
      log("Beginning the shutdown operation.", loglevel::info);
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
         log("Stopping accepting connections", loglevel::info);
         return;
      }

      fail(ec, "accept");
      return;
   }

   auto const n = next % std::size(arenas);
   std::make_shared< server_session
                   >(std::move(peer), arenas[n]->worker_)->accept();
   ++next;

   do_accept();
}

}


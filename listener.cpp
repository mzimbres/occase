#include "listener.hpp"

#include <iostream>
#include <algorithm>

#include <fmt/format.h>
#include <fmt/ostream.h>

#include "server_session.hpp"
#include "worker.hpp"

namespace rt
{

listener::listener(listener_cfg const& cfg)
: signals {ioc, SIGINT, SIGTERM}
, acceptor {ioc, {boost::asio::ip::tcp::v4(), cfg.port}}
{
   //acceptor.listen(256);
   auto const* fmt1 = "Binding server to {}";
   log(fmt::format(fmt1, acceptor.local_endpoint()), loglevel::info);

   auto worker_gen = [&cfg, i = -1]() mutable
      { return std::make_shared<worker>(cfg.mgr, ++i); };

   std::generate_n( std::back_inserter(workers)
                  , cfg.number_of_workers
                  , worker_gen);

   auto thread_gen = [](auto o)
      { return std::thread {[o](){ o->run();}}; };

   std::transform( std::begin(workers)
                 , std::end(workers)
                 , std::back_inserter(threads)
                 , thread_gen);
}

void listener::shutdown()
{
   acceptor.cancel();

   auto joiner = [](auto& o)
      { o.join(); };

   std::for_each(std::begin(threads), std::end(threads), joiner);
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
   auto const n = next % std::size(workers);
   acceptor.async_accept( workers[n]->get_io_context()
                        , [this](auto const& ec, auto socket)
                          { on_accept(ec, std::move(socket)); });
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

   auto const n = next % std::size(workers);
   std::make_shared< server_session
                   >( std::move(peer), workers[n])->accept();
   ++next;

   do_accept();
}

}


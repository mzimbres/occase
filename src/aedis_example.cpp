#include <deque>
#include <thread>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>

#include "aedis.hpp"

using namespace aedis;

auto const pub_handler = [](auto const& ec, auto const& data)
{
   if (ec)
     throw std::runtime_error(ec.message());

  std::cout << "=======> ";
   for (auto const& o : data)
     std::cout << o << " ";

   std::cout << std::endl;
};

void pub(session::config const& cfg, int count, char const* channel)
{
   net::io_context ioc;
   session pub_session(ioc, cfg);
   pub_session.set_msg_handler(pub_handler);
   for (auto i = 0; i < count; ++i)
      pub_session.send(publish(channel, std::to_string(i)));

   pub_session.run();

   ioc.run();
}

auto const sub_on_msg_handler =
   [i = 0]( auto const& ec, auto const& data) mutable
{
   if (ec) 
      throw std::runtime_error(ec.message());

   auto const n = std::stoi(data.back());
   if (n != i + 1)
      std::cout << "===============> Error." << std::endl;
   std::cout << "Counter: " << n << std::endl;

   i = n;

   //for (auto const& o : data)
   //   std::cout << o << " ";

   //std::cout << std::endl;
};

struct sub_arena {
   session s;

   sub_arena( net::io_context& ioc
            , session::config const& cfg
            , std::string channel)
   : s(ioc, cfg, "1")
   {
      s.set_msg_handler(sub_on_msg_handler);

      auto const on_conn_handler = [this, channel]()
         { s.send(subscribe(channel)); };

      s.set_on_conn_handler(on_conn_handler);
      s.run();
   }
};

void sub(session::config const& cfg, char const* channel)
{
   net::io_context ioc;
   sub_arena arena(ioc, cfg, channel);
   ioc.run();
}

void transaction(session::config const& cfg)
{
   net::io_context ioc;
   session ss(ioc, cfg);
   ss.set_msg_handler(pub_handler);

   auto c1 = multi()
           + incr("pub_counter")
           + publish("foo", "bar")
           + exec();

   ss.send(std::move(c1));

   ss.run();
   ioc.run();
}

void expire(session::config const& cfg)
{
   net::io_context ioc;
   session ss(ioc, cfg);
   ss.set_msg_handler(pub_handler);

   auto c1 = multi()
           + set("foo", "bar")
           + expire("foo", 10)
           + exec();

   ss.send(std::move(c1));

   ss.run();
   ioc.run();
}

void zadd(session::config const& cfg)
{
   net::io_context ioc;
   session ss(ioc, cfg);
   ss.set_msg_handler(pub_handler);

   auto c1 = zadd("foo", 1, "bar1")
           + zadd("foo", 2, "bar2")
           + zadd("foo", 3, "bar3")
           + zadd("foo", 4, "bar4")
           + zadd("foo", 5, "bar5");

   ss.send(c1);

   ss.run();
   ioc.run();
}

void zrangebyscore(session::config const& cfg)
{
   net::io_context ioc;
   session ss(ioc, cfg);
   ss.set_msg_handler(pub_handler);

   auto c1 = zrangebyscore("foo", 2, -1);

   ss.send(c1);

   ss.run();
   ioc.run();
}

void zrange(session::config const& cfg)
{
   net::io_context ioc;
   session ss(ioc, cfg);
   ss.set_msg_handler(pub_handler);

   ss.send(zrange("foo", 2, -1));

   ss.run();
   ioc.run();
}

void lrange(session::config const& cfg)
{
   net::io_context ioc;
   session ss(ioc, cfg);
   ss.set_msg_handler(pub_handler);

   auto c1 = lrange("foo", 0, -1);

   ss.send(c1);

   ss.run();
   ioc.run();
}

void read_msg_op(session::config const& cfg)
{
   net::io_context ioc;
   session ss(ioc, cfg);
   ss.set_msg_handler(pub_handler);

   auto c1 = multi()
           + lrange("foo", 0, -1)
           + del("foo")
           + exec();

   ss.send(c1);

   ss.run();
   ioc.run();
}

int main(int argc, char* argv[])
{
   try {
      if (argc == 1) {
         std::cerr << "Usage: " << argv[0] << " n host port" << std::endl;
         return 1;
      }

      session::config cfg;
      auto const n = std::stoi(argv[1]);

      if (argc == 3) {
         cfg.host = argv[2];
         cfg.port = argv[3];
      }

      char const* channel = "foo";

      switch (n) {
         case 0:
         case 1: pub(cfg, 10, channel); break;
         case 2: sub(cfg, channel); break;
         case 3: transaction(cfg); break;
         case 4: expire(cfg); break;
         case 5: zadd(cfg); break;
         case 6: zrangebyscore(cfg); break;
         case 7: zrange(cfg); break;
         case 8: lrange(cfg); break;
         case 9: read_msg_op(cfg); break;
         default:
            std::cerr << "Option not available." << std::endl;
      }
   } catch (std::exception const& e) {
      std::cerr << e.what() << "\n";
   }

   return 0;
}


#include <deque>
#include <thread>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>

#include <boost/asio.hpp>
#include <boost/program_options/options_description.hpp>
#include <boost/program_options/parsers.hpp>
#include <boost/program_options/variables_map.hpp>

#include "async_read_resp.hpp"
#include "redis_session.hpp"

using namespace rt::redis;

namespace po = boost::program_options;

// TODO: Implement all tests with arena to be able to test
// reconnection to redis properly.

auto const pub_handler = [](auto const& ec, auto const& data)
{
   if (ec)
     throw std::runtime_error(ec.message());

  std::cout << "=======> ";
   for (auto const& o : data)
     std::cout << o << " ";

   std::cout << std::endl;
};

void pub(session_cfg const& cfg, int count, char const* channel)
{
   boost::asio::io_context ioc;
   session pub_session(cfg, ioc, "1");
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
            , session_cfg const& cfg
            , std::string channel)
   : s(cfg, ioc, "1")
   {
      s.set_msg_handler(sub_on_msg_handler);

      auto const on_conn_handler = [this, channel]()
         { s.send(subscribe(channel)); };

      s.set_on_conn_handler(on_conn_handler);
      s.run();
   }
};

void sub(session_cfg const& cfg, char const* channel)
{
   net::io_context ioc;
   sub_arena arena(ioc, cfg, channel);
   ioc.run();
}

void transaction(session_cfg const& cfg)
{
   boost::asio::io_context ioc;
   session ss(cfg, ioc, "1");
   ss.set_msg_handler(pub_handler);

   auto c1 = multi()
           + incr("pub_counter")
           + publish("foo", "bar")
           + exec();

   ss.send(std::move(c1));

   ss.run();
   ioc.run();
}

void expire(session_cfg const& cfg)
{
   boost::asio::io_context ioc;
   session ss(cfg, ioc, "1");
   ss.set_msg_handler(pub_handler);

   auto c1 = multi()
           + set("foo", "bar")
           + expire("foo", 10)
           + exec();

   ss.send(std::move(c1));

   ss.run();
   ioc.run();
}

void zadd(session_cfg const& cfg)
{
   boost::asio::io_context ioc;
   session ss(cfg, ioc, "1");
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

void zrangebyscore(session_cfg const& cfg)
{
   boost::asio::io_context ioc;
   session ss(cfg, ioc, "1");
   ss.set_msg_handler(pub_handler);

   auto c1 = zrangebyscore("foo", 2, -1);

   ss.send(c1);

   ss.run();
   ioc.run();
}

void zrange(session_cfg const& cfg)
{
   boost::asio::io_context ioc;
   session ss(cfg, ioc, "1");
   ss.set_msg_handler(pub_handler);

   auto c1 = zrange("foo", 2, -1);

   ss.send(c1);

   ss.run();
   ioc.run();
}

void lrange(session_cfg const& cfg)
{
   boost::asio::io_context ioc;
   session ss(cfg, ioc, "1");
   ss.set_msg_handler(pub_handler);

   auto c1 = lrange("foo", 0, -1);

   ss.send(c1);

   ss.run();
   ioc.run();
}

void read_msg_op(session_cfg const& cfg)
{
   boost::asio::io_context ioc;
   session ss(cfg, ioc, "1");
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
      auto test = 0;
      auto count = 0;
      session_cfg cfg;
      po::options_description desc("Options");
      desc.add_options()
      ("help,h", "Produces help message")
      ( "port,p"
      , po::value<std::string>(&cfg.port)->default_value("6379")
      , "Redis server listening port."
      )

      ( "redis-sentinels"
      , po::value<std::vector<std::string>>(&cfg.sentinels)
      , "A list of sentinel addresses in the form ip1:port1 ip2:port2."
      )

      ("host,i"
      , po::value<std::string>(&cfg.host)->default_value("127.0.0.1")
      , "Redis server ip address."
      )

      ("Run mode,t"
      , po::value<int>(&test)->default_value(-1)
      , " 1 pub.\n"
        " 2 sub.\n"
        " 3 transaction.\n"
        " 4 expire.\n"
        " 5 zadd.\n"
        " 6 zrangebyscore.\n"
        " 7 zrange.\n"
        " 8 lrange.\n"
        " 9 read msg op.\n"
      )

      ("count,c"
      , po::value<int>(&count)->default_value(20)
      , "Count."
      )
      ;

      po::variables_map vm;        
      po::store(po::parse_command_line(argc, argv, desc), vm);
      po::notify(vm);    

      if (vm.count("help")) {
         std::cout << desc << "\n";
         return 0;
      }

      char const* channel = "foo";

      if (test == 1) {
         pub(cfg, count, channel);
         return 0;
      }

      if (test == 2) {
         sub(cfg, channel);
         return 0;
      }

      if (test == 3) {
         transaction(cfg);
         return 0;
      }

      if (test == 4) {
         expire(cfg);
         return 0;
      }

      if (test == 5) {
         zadd(cfg);
         return 0;
      }

      if (test == 6) {
         zrangebyscore(cfg);
         return 0;
      }

      if (test == 7) {
         zrange(cfg);
         return 0;
      }

      if (test == 8) {
         lrange(cfg);
         return 0;
      }

      if (test == 9) {
         read_msg_op(cfg);
         return 0;
      }

   } catch (std::exception& e) {
      std::cerr << "Exception: " << e.what() << "\n";
   }

   return 0;
}


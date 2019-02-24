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

#include "redis_session.hpp"
#include "resp.hpp"

using namespace rt::redis;

namespace po = boost::program_options;

// TODO: Implement all tests with arena to be able to test
// reconnection to redis properly.

auto const pub_handler = []( auto const& ec
                       , auto const& data
                       , auto const& cmd)
{
     if (ec)
        throw std::runtime_error(ec.message());

     for (auto const& o : data)
        std::cout << "Receivers: " << o << " ";
     
     std::cout << std::endl;
};

void pub(session_cf const& cf, int count, char const* channel)
{
   boost::asio::io_context ioc;
   session pub_session(cf, ioc, request::unknown);
   pub_session.set_msg_handler(pub_handler);
   for (auto i = 0; i < count; ++i) {
      auto const msg = std::to_string(i);
      req_data r
      { request::publish
      , gen_resp_cmd(cmd::publish, {channel, msg})
      , "" 
      };
      pub_session.send(std::move(r));
      //std::cout << "Sent: " << msg << std::endl;
   }
   pub_session.run();

   ioc.run();
}

auto const sub_on_msg_handler = [i = 0]( auto const& ec
                                       , auto const& data
                                       , auto const& cmd) mutable
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
            , session_cf const& cf
            , std::string channel)
   : s(cf, ioc, request::unsolicited_publish)
   {
      s.set_msg_handler(sub_on_msg_handler);

      auto const on_conn_handler = [this, channel]()
      {
         req_data r
         { request::subscribe
         , gen_resp_cmd(cmd::subscribe, {channel})
         , ""
         };

         s.send(std::move(r));
      };

      s.set_on_conn_handler(on_conn_handler);
      s.run();
   }
};

void sub(session_cf const& cf, char const* channel)
{
   net::io_context ioc;
   sub_arena arena(ioc, cf, channel);
   ioc.run();
}

void pubsub(session_cf const& cf, int count, char const* channel)
{
   boost::asio::io_context ioc;

   session pub_session(cf, ioc, request::unknown);
   pub_session.set_msg_handler(pub_handler);
   for (auto i = 0; i < count; ++i) {
      auto const msg = std::to_string(i);
      req_data r
      { request::publish
      , gen_resp_cmd(cmd::publish, {channel, msg})
      , ""
      };
      pub_session.send(std::move(r));
      //std::cout << "Sent: " << msg << std::endl;
   }

   pub_session.run();

   session sub_session(cf, ioc, request::unsolicited_publish);
   sub_session.set_msg_handler(sub_on_msg_handler);
   req_data r
   { request::subscribe
   , gen_resp_cmd(cmd::subscribe, {channel})
   , ""
   };
   sub_session.send(std::move(r));
   sub_session.run();
   ioc.run();
}

int main(int argc, char* argv[])
{
   try {
      auto test = 0;
      auto count = 0;
      session_cf cf;
      po::options_description desc("Options");
      desc.add_options()
      ("help,h", "Produces help message")
      ( "port,p"
      , po::value<std::string>(&cf.port)->default_value("6379")
      , "Redis server listening port."
      )
      ("host,i"
      , po::value<std::string>(&cf.host)->default_value("127.0.0.1")
      , "Redis server ip address."
      )
      ("Run mode,t"
      , po::value<int>(&test)->default_value(-1)
      , " 1 pub.\n"
        " 2 sub.\n"
        " 3 pubsub."
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
         pub(cf, count, channel);
         return 0;
      }

      if (test == 2) {
         sub(cf, channel);
         return 0;
      }

      if (test == 3) {
         pubsub(cf, count, channel);
         return 0;
      }

   } catch (std::exception& e) {
      std::cerr << "Exception: " << e.what() << "\n";
   }

   return 0;
}


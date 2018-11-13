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

using namespace rt;

namespace po = boost::program_options;

void pub(redis_session_cf const& cf, int count)
{
   auto const pub_handler = []( auto const& ec
                          , auto const& data
                          , auto const& cmd)
   {
        if (ec)
           throw std::runtime_error(ec.message());

        //for (auto const& o : data)
        //   std::cout << o << " ";
        //
        //std::cout << std::endl;
   };

   boost::asio::io_context ioc;
   redis_session pub_session(cf, ioc);
   pub_session.set_on_msg_handler(pub_handler);
   for (auto i = 0; i < count; ++i) {
      auto const msg = std::to_string(i);
      pub_session.send(gen_resp_cmd( redis_cmd::publish
                                   , {"foo", msg}));
      std::cout << "Sent: " << msg << std::endl;
   }
   pub_session.run();

   ioc.run();
}

void sub(redis_session_cf const& cf)
{
   auto const sub_handler = []( auto const& ec
                              , auto const& data
                              , auto const& cmd)
   {
        if (ec) 
           throw std::runtime_error(ec.message());

        for (auto const& o : data)
           std::cout << o << " ";
        
        std::cout << std::endl;
   };

   boost::asio::io_context ioc;
   redis_session sub_session(cf, ioc);
   sub_session.set_on_msg_handler(sub_handler);
   sub_session.send(gen_resp_cmd(redis_cmd::subscribe, {"foo"}));
   sub_session.run();
   ioc.run();
}

int main(int argc, char* argv[])
{
   try {
      auto test = 0;
      auto count = 0;
      redis_session_cf cf;
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
      , "1 for pub, 2 for sub."
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

      if (test == 1) {
         pub(cf, count);
         return 0;
      }

      if (test == 2) {
         sub(cf);
         return 0;
      }

      //pub_session.send(gen_resp_cmd(redis_cmd::incrby, {"foo", "3"}));
      //pub_session.send(gen_resp_cmd(redis_cmd::get, {"foo"}));
      //pub_session.send(gen_resp_cmd( redis_cmd::ping
      //                             , {"Arbitrary message."}));
      //pub_session.send(gen_resp_cmd( redis_cmd::publish
      //                             , {"foo", "Message."}));

      //pub_session.send(gen_resp_cmd( redis_cmd::ping
      //                             , {"Arbitrary message2."}));
      //pub_session.send(gen_resp_cmd( redis_cmd::lpop
      //                             , {"nonsense"}));
      //pub_session.send(gen_resp_cmd( redis_cmd::ping
      //                             , {"Arbitrary message3."}));
      //pub_session.send(gen_resp_cmd( redis_cmd::rpush
      //                             , {"nonsense", "one", "two", "three"}));
      //pub_session.send(gen_resp_cmd( redis_cmd::lrange
      //                             , {"nonsense", "0", "-1"}));
   } catch (std::exception& e) {
      std::cerr << "Exception: " << e.what() << "\n";
   }

   return 0;
}


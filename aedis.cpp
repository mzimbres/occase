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

int main(int argc, char* argv[])
{
   try {
      redis_session_cf cf;
      po::options_description desc("Options");
      desc.add_options()
      ("help,h", "Produces help message")
      ( "port,p"
      , po::value<std::string>(&cf.port)->default_value("6379")
      , "Server listening port."
      )
      ("host,i"
      , po::value<std::string>(&cf.host)->default_value("127.0.0.1")
      , "Server ip address."
      )
      ;

      po::variables_map vm;        
      po::store(po::parse_command_line(argc, argv, desc), vm);
      po::notify(vm);    

      if (vm.count("help")) {
         std::cout << desc << "\n";
         return 0;
      }

      boost::asio::io_context ioc;
      redis_session sub_session(cf, ioc);

      auto const handler = []( auto const& ec
                             , auto const& data
                             , auto const& cmd)
      {
           if (ec) {
              std::cout << ec.message() << std::endl;
              return;
           }

           for (auto const& o : data)
              std::cout << o << " ";
           
           std::cout << std::endl;
      };

      sub_session.set_on_msg_handler(handler);
      sub_session.send(gen_resp_cmd(redis_cmd::subscribe, {"foo"}));
      sub_session.run();

      redis_session pub_session(cf, ioc);
      pub_session.set_on_msg_handler(handler);

      pub_session.send(gen_resp_cmd(redis_cmd::set, {"foo", "20"}));
      pub_session.send(gen_resp_cmd(redis_cmd::incrby, {"foo", "3"}));
      pub_session.send(gen_resp_cmd(redis_cmd::get, {"foo"}));
      pub_session.send(gen_resp_cmd( redis_cmd::ping
                                   , {"Arbitrary message."}));
      for (auto i = 0; i < 2; ++i)
         pub_session.send(gen_resp_cmd( redis_cmd::publish
                                      , {"foo", "Message."}));

      pub_session.send(gen_resp_cmd( redis_cmd::ping
                                   , {"Arbitrary message2."}));
      pub_session.send(gen_resp_cmd( redis_cmd::lpop
                                   , {"nonsense"}));
      pub_session.send(gen_resp_cmd( redis_cmd::ping
                                   , {"Arbitrary message3."}));
      pub_session.send(gen_resp_cmd( redis_cmd::rpush
                                   , {"nonsense", "one", "two", "three"}));
      pub_session.send(gen_resp_cmd( redis_cmd::lrange
                                   , {"nonsense", "0", "-1"}));
      pub_session.run();
      ioc.run();
   } catch (std::exception& e) {
      std::cerr << "Exception: " << e.what() << "\n";
   }

   return 0;
}


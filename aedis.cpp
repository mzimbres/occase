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

using namespace aedis;

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
      auto session = std::make_shared<redis_session>(cf, ioc);

      auto const sub_handler = [](auto ec, auto&& data)
      {
           if (ec) {
              std::cout << ec.message() << std::endl;
              return;
           }

           for (auto const& o : data)
              std::cout << o << " ";
           
           std::cout << std::endl;
      };

      session->set_msg_handler(sub_handler);

      session->send(gen_resp_cmd("SET", {"foo", "20"}));
      session->send(gen_resp_cmd("INCRBY", {"foo", "3"}));
      session->send(gen_resp_cmd("GET", {"foo"}));
      session->send(gen_resp_cmd("PING", {"Arbitrary message."}));
      session->send(gen_resp_cmd("SUBSCRIBE", {"foo"}));

      session->run();
      ioc.run();
   } catch (std::exception& e) {
      std::cerr << "Exception: " << e.what() << "\n";
   }

   return 0;
}


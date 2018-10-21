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

      auto const sub_handler = [](auto ec, auto data)
      {
           if (ec) {
              std::cout << ec.message() << std::endl;
              return;
           }

           auto const asw = get_bulky_string_array( data.data()
                                                  , std::size(data));
           std::cout << "(array) ";
           for (auto const& o : asw)
              std::cout << o << " ";
           
           std::cout << std::endl;
      };

      session->set_sub_handler(sub_handler);

      interaction i1
      { gen_resp_cmd("SET", {"foo", "20"})
      , [](auto ec, auto payload)
        {
           if (ec) {
              std::cout << ec.message() << std::endl;
              return;
           }

           std::cout << "(simple string) "
                     << get_simple_string(payload.data())
                     << std::endl;
        }
      };

      interaction i2
      { gen_resp_cmd("INCRBY", {"foo", "3"})
      , [](auto ec, auto payload)
        {
           if (ec) {
              std::cout << ec.message() << std::endl;
              return;
           }

           std::cout << "(integer) " << get_int(payload.data()) << std::endl;
        }
      };

      interaction i3
      { gen_resp_cmd("GET", {"foo"})
      , [](auto ec, auto payload)
        {
           if (ec) {
              std::cout << ec.message() << std::endl;
              return;
           }

           std::cout << "(bulky string) "
                     << get_bulky_string(payload.data(), std::size(payload))
                     << std::endl;
        }
      };

      interaction i4
      { gen_resp_cmd("PING", {"Arbitrary message."})
      , [](auto ec, auto payload)
        {
           if (ec) {
              std::cout << ec.message() << std::endl;
              return;
           }

           std::cout << "(bulky string) "
                     << get_bulky_string(payload.data(), std::size(payload))
                     << std::endl;
        }
      };

      interaction i5
      { gen_resp_cmd("SUBSCRIBE", {"foo"})
      , [](auto ec, auto payload)
        {
           if (ec) {
              std::cout << ec.message() << std::endl;
              return;
           }

           auto const asw = get_bulky_string_array( payload.data()
                                                  , std::size(payload));
           std::cout << "(array) ";
           for (auto const& o : asw)
              std::cout << o << " ";
           
           std::cout << std::endl;
        }
      };

      session->send(std::move(i1));
      session->send(std::move(i2));
      session->send(std::move(i3));
      session->send(std::move(i4));
      session->send(std::move(i5));

      session->run();
      ioc.run();
   } catch (std::exception& e) {
      std::cerr << "Exception: " << e.what() << "\n";
   }

   return 0;
}


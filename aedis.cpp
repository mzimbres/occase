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

struct test_action {
   std::string cmd;
   std::string expected;
   int repeat = 0;
   std::shared_ptr<redis_session> session;
   void operator()( boost::system::error_code ec
                  , std::string response) const
   {
      if (ec) {
         std::string error_msg = cmd + ": test fails.";
         throw std::runtime_error(error_msg);
      }

      auto const str = get_simple_string(response);
      if (str != expected) {
         std::string error_msg = cmd + ": test fails. ";
         error_msg += "Expects ";
         error_msg += expected + ". Received ";
         error_msg += str;
         throw std::runtime_error(error_msg);
      }

      if (repeat == 0) {
         session->close();
         std::cout << cmd <<": test ok." << std::endl;
         return;
      }

      interaction tmp { gen_bulky_string(cmd, {})
                      , test_action {cmd, expected, repeat - 1, session}};

      session->send(std::move(tmp));
   }
};

void test_ping(redis_session_cf const cf)
{
   boost::asio::io_context ioc;

   auto session = std::make_shared<redis_session>(cf, ioc);
   interaction a1 { gen_bulky_string({"PING"}, {})
                  , test_action {"PING", "PONG", 1, session}};

   session->send(std::move(a1));
   session->run();

   ioc.run();
}

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

      //test_ping(cf);
      boost::asio::io_context ioc;
      auto session = std::make_shared<redis_session>(cf, ioc);

      interaction i1
      { gen_bulky_string("SET", {"foo", "20"})
      , [](auto ec, auto payload)
        {
           if (ec) {
              std::cout << ec.message() << std::endl;
              return;
           }

           std::cout << "(simple string) " << get_simple_string(payload)
                     << std::endl;
        }
      };

      interaction i2
      { gen_bulky_string("INCRBY", {"foo", "3"})
      , [](auto ec, auto payload)
        {
           if (ec) {
              std::cout << ec.message() << std::endl;
              return;
           }

           std::cout << "(integer) " << get_int(payload) << std::endl;
        }
      };

      interaction i3
      { gen_bulky_string("GET", {"foo"})
      , [](auto ec, auto payload)
        {
           if (ec) {
              std::cout << ec.message() << std::endl;
              return;
           }

           std::cout << "(bulky string) " << get_bulky_string(payload)
                     << std::endl;
        }
      };

      interaction i4
      { gen_bulky_string("PING", {"Arbitrary message."})
      , [](auto ec, auto payload)
        {
           if (ec) {
              std::cout << ec.message() << std::endl;
              return;
           }

           std::cout << "(bulky string) " << get_bulky_string(payload)
                     << std::endl;
        }
      };

      session->send(std::move(i1));
      session->send(std::move(i2));
      session->send(std::move(i3));
      session->send(std::move(i4));

      session->run();
      ioc.run();
   } catch (std::exception& e) {
      std::cerr << "Exception: " << e.what() << "\n";
   }

   return 0;
}


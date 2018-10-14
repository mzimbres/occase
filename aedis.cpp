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

struct aedis_op {
   std::string ip;
   std::string port;
};

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

void test_ping(aedis_op const op)
{
   boost::asio::io_context ioc;

   boost::asio::ip::tcp::resolver resolver(ioc);
   auto endpoints = resolver.resolve(op.ip, op.port);

   auto session = std::make_shared<redis_session>(ioc, endpoints);
   interaction a1 { gen_bulky_string({"PING"}, {})
                  , test_action {"PING", "PONG", 1000, session}};

   session->send(std::move(a1));
   session->run();

   ioc.run();
}

int main(int argc, char* argv[])
{
   try {
      aedis_op op;
      po::options_description desc("Options");
      desc.add_options()
      ("help,h", "Produces help message")
      ( "port,p"
      , po::value<std::string>(&op.port)->default_value("6379")
      , "Server listening port."
      )
      ("ip,i"
      , po::value<std::string>(&op.ip)->default_value("127.0.0.1")
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

      test_ping(op);
      boost::asio::io_context ioc;

      boost::asio::ip::tcp::resolver resolver(ioc);
      auto endpoints = resolver.resolve(op.ip, op.port);

      auto session = std::make_shared<redis_session>(ioc, endpoints);
      auto const ab1 = [](auto ec, auto payload)
      {
         if (ec) {
            std::cout << "Error while reading." << std::endl;
            return;
         }

         std::cout << "(simple string) " << get_simple_string(payload)
                   << std::endl;
      };

      interaction b {gen_bulky_string("SET", {"foo", "20"}), ab1};
      session->send(std::move(b));

      auto const ac1 = [](auto ec, auto payload)
      {
         if (ec) {
            std::cout << "Error while reading." << std::endl;
            return;
         }
         std::cout << "(integer) " << get_int(payload) << std::endl;
      };

      interaction c1 {gen_bulky_string("INCRBY", {"foo", "3"}), ac1};
      session->send(std::move(c1));

      auto const ad1 = [](auto ec, auto payload)
      {
         if (ec) {
            std::cout << "Error while reading." << std::endl;
            return;
         }

         std::cout << "(bulky string) " << get_bulky_string(payload)
                   << std::endl;
      };

      interaction d1 {gen_bulky_string("GET", {"foo"}) , ad1};
      session->send(std::move(d1));

      session->run();
      ioc.run();
   } catch (std::exception& e) {
      std::cerr << "Exception: " << e.what() << "\n";
   }

   return 0;
}


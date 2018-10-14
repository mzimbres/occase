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

//auto gen_ping_cmd(std::string msg)
//{
//   std::vector<char> cmd = "*2";
//   if (std::empty(msg))
//      cmd = "*1";
//
//   cmd += "\r\n$4\r\nPING\r\n";
//
//   if (!std::empty(msg))
//      cmd += msg;
//}

void test_ping(aedis_op const op)
{
   boost::asio::io_context ioc;

   boost::asio::ip::tcp::resolver resolver(ioc);
   auto endpoints = resolver.resolve(op.ip, op.port);

   auto session = std::make_shared<redis_session>(ioc, endpoints);
   auto const action = [session](auto ec, auto response)
   {
      if (ec)
         throw std::runtime_error("test_ping: Error");

      auto const str = get_simple_string(response);
      if (str == "PONG") {
         std::cout << "test_ping: ok." << std::endl;
      } else {
         std::cout << "test_ping: fail." << std::endl;
         std::cout << "Expected ok, received: " << str << std::endl;
      }

      //resp_response resp(std::move(response));
      //resp.process_response();
      session->close();
   };

   interaction a1 { {"*1\r\n$4\r\nPING\r\n"} , action , false};
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
      auto const action = [](auto ec, auto payload)
      {
         if (ec) {
            std::cout << "Error while reading." << std::endl;
            return;
         }
         resp_response resp(std::move(payload));
         resp.process_response();
      };

      interaction a1 { {"*1\r\n$4\r\nPING\r\n"} , action , false};
      session->send(std::move(a1));
      interaction a2 { {"PING\r\n"} , action , false};
      session->send(std::move(a2));
      interaction a3 { {"PING\r\n"} , action , false};
      session->send(std::move(a3));
      interaction a4 { {"PING\r\n"} , action , false};
      session->send(std::move(a4));

      interaction b { {"*3\r\n$3\r\nSET\r\n$3\r\nfoo\r\n$2\r\n20\r\n"}
                    , action , false};
      session->send(std::move(b));

      interaction c1 { {"*3\r\n$6\r\nINCRBY\r\n$3\r\nfoo\r\n$1\r\n3\r\n"}
                    , action , false};
      session->send(std::move(c1));

      interaction c2 { {"*3\r\n$6\r\nINCRBY\r\n$3\r\nfoo\r\n$1\r\n3\r\n"}
                    , action , false};
      session->send(std::move(c2));

      interaction c3 { {"*3\r\n$6\r\nINCRBY\r\n$3\r\nfoo\r\n$1\r\n3\r\n"}
                    , action , false};
      session->send(std::move(c3));

      interaction d1 { {"*2\r\n$3\r\nGET\r\n$3\r\nfoo\r\n"}
                    , action , false};
      session->send(std::move(d1));

      session->run();

      //std::thread thread([&](){ioc.run();});

      //char line[1024];
      //while (std::cin.getline(line, std::size(line))) {
      //   session->write(line);
      //}

      //session->close();
      //thread.join();
      ioc.run();
   } catch (std::exception& e) {
      std::cerr << "Exception: " << e.what() << "\n";
   }

   return 0;
}


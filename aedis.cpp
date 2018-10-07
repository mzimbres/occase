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

namespace po = boost::program_options;

struct aedis_op {
   std::string ip;
   std::string port;
};

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

      boost::asio::io_context ioc;

      boost::asio::ip::tcp::resolver resolver(ioc);
      auto endpoints = resolver.resolve(op.ip, op.port);

      auto session = std::make_shared<redis_session>(ioc, endpoints);
      session->run();

      std::thread thread([&](){ioc.run();});

      char line[1024];
      while (std::cin.getline(line, std::size(line))) {
         session->write(line);
      }

      session->close();
      thread.join();
   } catch (std::exception& e) {
      std::cerr << "Exception: " << e.what() << "\n";
   }

   return 0;
}


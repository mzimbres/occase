#include <deque>
#include <thread>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>

#include <boost/asio.hpp>

#include "redis_session.hpp"

int main(int argc, char* argv[])
{
  try {
    if (argc != 3) {
      std::cerr << "Usage: redis_session <host> <port>\n";
      return 1;
    }

    boost::asio::io_context ioc;

    boost::asio::ip::tcp::resolver resolver(ioc);
    auto endpoints = resolver.resolve(argv[1], argv[2]);

    redis_session client(ioc, endpoints);

    std::thread thread([&](){ioc.run();});

    char line[1024];
    while (std::cin.getline(line, std::size(line)))
    {
      client.write("PING");
    }

    client.close();
    thread.join();
  } catch (std::exception& e) {
    std::cerr << "Exception: " << e.what() << "\n";
  }

  return 0;
}


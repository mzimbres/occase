#include <deque>
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

    redis_session c(ioc, endpoints);

    //boost::thread t(boost::bind(&boost::asio::io_context::run, &ioc));

    //char line[chat_message::max_body_length + 1];
    //while (std::cin.getline(line, chat_message::max_body_length + 1))
    //{
    //  using namespace std; // For strlen and memcpy.
    //  chat_message msg;
    //  msg.body_length(strlen(line));
    //  memcpy(msg.body(), line, msg.body_length());
    //  msg.encode_header();
    //  c.write(msg);
    //}

    //c.close();
    //t.join();
  } catch (std::exception& e) {
    std::cerr << "Exception: " << e.what() << "\n";
  }

  return 0;
}


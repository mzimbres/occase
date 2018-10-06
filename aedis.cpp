#include <deque>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>

#include <boost/asio.hpp>
#include <boost/bind.hpp>

using boost::asio::ip::tcp;

class redis_client :
  public std::enable_shared_from_this<redis_client> {
private:
   boost::asio::io_context& ioc;
   tcp::socket socket;
   std::string message;
   std::deque<std::string> write_queue;
   tcp::resolver::results_type endpoints;

public:
   redis_client( boost::asio::io_context& ioc_
               , tcp::resolver::results_type endpoints_)
   : ioc(ioc_)
   , socket(ioc_)
   , endpoints(endpoints_)
   { }

   void run()
   {
      auto const handler = [p = shared_from_this()](auto ec, auto Iterator)
      {
         p->on_connect(ec);
      };

      boost::asio::async_connect(socket, endpoints, handler);
   }

   void write(std::string msg)
   {
      auto const handler = [p = shared_from_this(), m = std::move(msg)]()
      {
         p->do_write(std::move(m));
      };

      boost::asio::post(ioc, handler);
   }

   void close()
   {
    boost::asio::post(ioc,
        boost::bind(&redis_client::do_close, this));
   }

private:

   void on_connect(boost::system::error_code const& ec)
   {
      if (ec) {
         std::cout << "ec" << std::endl;
         return;
      }

      handle_read_body({}, 0);
   }

   void handle_read_body( boost::system::error_code const& ec
                        , std::size_t n)
   {
      if (ec) {
        do_close();
        return;
      }

      if (!std::empty(message))
         std::cout << message << "\n";

      auto const handler = [p = shared_from_this()](auto ec, auto n)
      {
         p->handle_read_body(ec, n);
      };

      boost::asio::async_read( socket
                             , boost::asio::buffer(message)
                             , handler);
   }

   void do_write(std::string msg)
   {
      auto const write_in_progress = !write_queue.empty();
      write_queue.push_back(std::move(msg));

      if (!write_in_progress) {
         auto const handler = [p = shared_from_this()](auto ec, auto n)
         {
            p->on_write(ec, n);
         };

         boost::asio::async_write( socket
                                 , boost::asio::buffer(write_queue.front())
                                 , handler);
      }
   }

   void on_write(const boost::system::error_code& ec, std::size_t n)
   {
      if (ec) {
        do_close();
        return;
      }

      write_queue.pop_front();

      if (!write_queue.empty()) {
         auto const handler = [p = shared_from_this()](auto ec, auto n)
         {
            p->on_write(ec, n);
         };

         boost::asio::async_write( socket
                                 , boost::asio::buffer(write_queue.front())
                                 , handler);
      }
   }

   void do_close()
   {
      socket.close();
   }
};

int main(int argc, char* argv[])
{
  try
  {
    if (argc != 3)
    {
      std::cerr << "Usage: redis_client <host> <port>\n";
      return 1;
    }

    boost::asio::io_context ioc;

    tcp::resolver resolver(ioc);
    tcp::resolver::results_type endpoints = resolver.resolve(argv[1], argv[2]);

    redis_client c(ioc, endpoints);

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
  }
  catch (std::exception& e)
  {
    std::cerr << "Exception: " << e.what() << "\n";
  }

  return 0;
}


#include <thread>
#include <mutex>
#include <vector>
#include <memory>
#include <string>
#include <cstdlib>
#include <sstream>
#include <iostream>
#include <functional>

#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>

#include "config.hpp"

namespace websocket = boost::beast::websocket;

void fail(boost::system::error_code ec, char const* what)
{
   std::cerr << what << ": " << ec.message() << "\n";
}

class session : public std::enable_shared_from_this<session> {
private:
   using work_type =
      boost::asio::executor_work_guard<
         boost::asio::io_context::executor_type>;

   tcp::resolver resolver;
   websocket::stream<tcp::socket> ws;
   boost::beast::multi_buffer buffer;
   std::string host {"127.0.0.1"};
   std::string text;
   std::mutex mutex;
   work_type work;
   std::string tel;
   int id = -1;
   std::vector<int> groups;

   void write(std::string msg)
   {
      text = std::move(msg);
      auto handler = [p = shared_from_this()](auto ec, auto res)
      { p->on_write(ec, res); };

      ws.async_write(boost::asio::buffer(text), handler);
   }

   void close()
   {
      auto handler = [p = shared_from_this()](auto ec)
      { p->on_close(ec); };

      ws.async_close(websocket::close_code::normal, handler);
   }

   void on_resolve( boost::system::error_code ec
                  , tcp::resolver::results_type results)
   {
      if (ec)
         return fail(ec, "resolve");

      auto handler = std::bind(
               &session::on_connect,
               shared_from_this(),
               std::placeholders::_1);

      // This does not work, why?
      //auto handler2 = [p = shared_from_this()](auto ec)
      //{ p->on_connect(ec); };

      // Make the connection on the IP address we get from a lookup
      boost::asio::async_connect(
            ws.next_layer(),
            results.begin(),
            results.end(), handler);
   }

   void on_connect(boost::system::error_code ec)
   {
      if (ec)
         return fail(ec, "connect");

      auto handler = [p = shared_from_this()](auto ec)
      { p->on_handshake(ec); };

      // Perform the websocket handshake
      ws.async_handshake(host, "/", handler);
   }

   void on_handshake(boost::system::error_code ec)
   {
      if (ec)
         return fail(ec, "handshake");

      auto handler = [p = shared_from_this()](auto ec, auto res)
      { p->on_read(ec, res); };

      ws.async_read(buffer, handler);
   }

   void on_write( boost::system::error_code ec
                , std::size_t bytes_transferred)
   {
      boost::ignore_unused(bytes_transferred);

      if (ec)
         return fail(ec, "write");

      //auto handler = [p = shared_from_this()](auto ec, auto res)
      //{ p->on_read(ec, res); };

      //ws.async_read(buffer, handler);
   }

   void on_read( boost::system::error_code ec
               , std::size_t bytes_transferred)
   {
      try {
         boost::ignore_unused(bytes_transferred);

         if (ec)
            return fail(ec, "read");

         json j;
         std::stringstream ss;
         ss << boost::beast::buffers(buffer.data());
         ss >> j;
         buffer.consume(buffer.size());

         auto cmd = j["cmd"].get<std::string>();

         if (cmd == "login_ack") {
            auto log_res = j["result"].get<std::string>();
            if (log_res == "ok") {
               id = j["user_idx"].get<int>();
               std::cout << "Client: assigning id " << id << std::endl;
            }
         }

         if (cmd == "create_group_ack") {
            auto log_res = j["result"].get<std::string>();
            if (log_res == "ok") {
               auto group_idx = j["group_idx"].get<int>();
               groups.push_back(group_idx);
               std::cout << "Client: adding group " << group_idx << std::endl;
            }
         }

         if (cmd == "join_group_ack") {
            auto log_res = j["result"].get<std::string>();
            std::cout << "Client: join group ack" << log_res << std::endl;
         }

      } catch (std::exception const& e) {
         std::cerr << "Error: " << e.what() << std::endl;
      }

      auto handler = [p = shared_from_this()](auto ec, auto res)
      { p->on_read(ec, res); };

      ws.async_read(buffer, handler);
   }

   void on_close(boost::system::error_code ec)
   {
      if (ec)
         return fail(ec, "close");

      std::cout << "Connection is closed gracefully"
                << std::endl;
      work.reset();
   }

   void send_msg(std::string msg)
   {
      auto handler = [p = shared_from_this(), msg = std::move(msg)]()
      { p->write(std::move(msg)); };

      boost::asio::post(resolver.get_executor(), handler);
   }

public:
   explicit
   session( boost::asio::io_context& ioc
          , std::string tel_)
   : resolver(ioc)
   , ws(ioc)
   , work(boost::asio::make_work_guard(ioc))
   , tel(std::move(tel_))
   { }

   void login()
   {
      json j;
      j["cmd"] = "login";
      j["name"] = "Marcelo Zimbres";
      j["tel"] = tel;

      send_msg(j.dump());
   }

   void create_group()
   {
      json j;
      j["cmd"] = "create_group";
      j["from"] = id;

      send_msg(j.dump());
   }

   void join_group()
   {
      json j;
      j["cmd"] = "join_group";
      j["from"] = id;
      j["group_idx"] = 0;

      send_msg(j.dump());
   }

   void exit()
   {
      auto handler = [p = shared_from_this()]()
      { p->close(); };

      boost::asio::post(resolver.get_executor(), handler);
   }

   void run()
   {
      char const* port = "8080";

      auto handler = [p = shared_from_this()](auto ec, auto res)
      { p->on_resolve(ec, res); };

      // Look up the domain name
      resolver.async_resolve(host, port, handler);
   }
};

struct prompt_usr {
   std::shared_ptr<session> p;
   void operator()() const
   {
      for (;;) {
         std::cout << "Type a command: \n\n"
                   << "  1: Login.\n"
                   << "  2: Create group.\n"
                   << "  3: Join group.\n"
                   << "  4: Send group message.\n"
                   << "  5: Exit.\n"
                   << std::endl;
         auto cmd = -1;
         std::cin >> cmd;
         std::string str;

         if (cmd == 1) {
            p->login();
            continue;
         }
         
         if (cmd == 2) {
            p->create_group();
            continue;
         }
         
         if (cmd == 3) {
            p->join_group();
            continue;
         }
         
         if (cmd == 4) {
            str = "cmd3";
            continue;
         }
         
         if (cmd == -1) {
            p->exit();
            break;
         }
      }
   }
};

int main(int argc, char* argv[])
{
   if (argc != 2) {
      std::cerr << "Please, provide a user id." << std::endl;
      return EXIT_FAILURE;
   }

   boost::asio::io_context ioc;

   auto p = std::make_shared<session>(ioc, argv[1]);

   std::thread thr {prompt_usr {p}};

   p->run();
   ioc.run();
   thr.join();

   return EXIT_SUCCESS;
}


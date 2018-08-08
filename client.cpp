#include <thread>
#include <mutex>
#include <memory>
#include <string>
#include <cstdlib>
#include <iostream>
#include <functional>

#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>

#include <nlohmann/json.hpp>

using tcp = boost::asio::ip::tcp;
using json = nlohmann::json;

namespace websocket = boost::beast::websocket;

void fail(boost::system::error_code ec, char const* what)
{
   std::cerr << what << ": " << ec.message() << "\n";
}

class session : public std::enable_shared_from_this<session> {
private:
   tcp::resolver resolver;
   websocket::stream<tcp::socket> ws;
   boost::beast::multi_buffer buffer;
   std::string host {"127.0.0.1"};
   std::string text;
   int id = -1;
   std::mutex mutex;
   using work_type =
      boost::asio::executor_work_guard<
         boost::asio::io_context::executor_type>;
   work_type work;

public:
   // Resolver and socket require an io_context
   explicit
   session(boost::asio::io_context& ioc)
   : resolver(ioc)
   , ws(ioc)
   , work(boost::asio::make_work_guard(ioc))
   { }

   void send_msg(std::string msg)
   {
      auto handler = [p = shared_from_this(), msg = std::move(msg)]()
      { p->write(std::move(msg)); };

      boost::asio::post(resolver.get_executor(), handler);
   }

   void write(std::string msg)
   {
      text = std::move(msg);
      auto handler = [p = shared_from_this()](auto ec, auto res)
      { p->on_write(ec, res); };

      ws.async_write(boost::asio::buffer(text), handler);
   }

   void exit()
   {
      auto handler = [p = shared_from_this()]()
      { p->close(); };

      boost::asio::post(resolver.get_executor(), handler);
   }

   void close()
   {
      auto handler = [p = shared_from_this()](auto ec)
      { p->on_close(ec); };

      ws.async_close(websocket::close_code::normal, handler);
   }

   void run()
   {
      char const* port = "8080";

      auto handler = [p = shared_from_this()](auto ec, auto res)
      { p->on_resolve(ec, res); };

      // Look up the domain name
      resolver.async_resolve(host, port, handler);
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
   }

   void on_write( boost::system::error_code ec
                , std::size_t bytes_transferred)
   {
      boost::ignore_unused(bytes_transferred);

      if (ec)
         return fail(ec, "write");

      auto handler = [p = shared_from_this()](auto ec, auto res)
      { p->on_read(ec, res); };

      ws.async_read(buffer, handler);
   }

   void on_read( boost::system::error_code ec
               , std::size_t bytes_transferred)
   {
      boost::ignore_unused(bytes_transferred);

      if (ec)
         return fail(ec, "read");

      std::cout << boost::beast::buffers(buffer.data())
                << std::endl;
      buffer.consume(buffer.size());

      //auto handler = [p = shared_from_this()](auto ec, auto res)
      //{ p->on_read(ec, res); };

      //ws.async_read(buffer, handler);
   }

   void on_close(boost::system::error_code ec)
   {
      if (ec)
         return fail(ec, "close");

      std::cout << "Connection is closed gracefully"
                << std::endl;
      work.reset();
   }
};

std::string get_cmd_str(int cmd)
{
   if (cmd == 1) {
      json j;
      j["cmd"] = "login";
      j["name"] = "Marcelo Zimbres";
      j["tel"] = "1";
      return j.dump();
   } 
   
   if (cmd == 2) {
      json j;
      j["cmd"] = "create_group";
      j["name"] = "Repasse de automóveis";
      return j.dump();
   }
   
   if (cmd == 3) {
      return "cmd3";
   }
   
   if (cmd == 4) {
      return "cmd3";
   }
   
   return {};
}

struct prompt_usr {
   std::shared_ptr<session> p;
   void operator()() const
   {
      for (;;) {
         std::cout << "Type a command: \n\n"
                   << "  1: Login.\n"
                   << "  2: Create group.\n"
                   << "  3: Send message.\n"
                   << "  4: Join group.\n"
                   << "  5: Exit.\n"
                   << std::endl;
         auto cmd = -1;
         std::cin >> cmd;
         auto str = get_cmd_str(cmd);
         if (str.empty()) {
            p->exit();
            break;
         }
         p->send_msg(str);
      }
   }
};

int main()
{
   boost::asio::io_context ioc;

   auto p = std::make_shared<session>(ioc);

   std::thread thr {prompt_usr {p}};

   p->run();
   ioc.run();
   thr.join();

   return EXIT_SUCCESS;
}


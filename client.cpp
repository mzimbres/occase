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

class session : public std::enable_shared_from_this<session>
{
private:
   tcp::resolver resolver;
   websocket::stream<tcp::socket> ws;
   boost::beast::multi_buffer buffer;
   std::string host {"127.0.0.1"};
   std::string text;

public:
   // Resolver and socket require an io_context
   explicit
   session(boost::asio::io_context& ioc)
   : resolver(ioc)
   , ws(ioc)
   { }

   void prompt_for_msg()
   {
      int cmd = -1;
      while (cmd == -1) {
         std::cout << "Type a command: \n\n"
                   << "  1: Login.\n"
                   << "  2: Send message.\n"
                   << "  3: Create group.\n"
                   << std::endl;
         std::cin >> cmd;
         if (cmd == 1) {
            json j;
            j["cmd"] = "login";
            j["name"] = "Marcelo Zimbres";
            j["tel"] = "1";
            text = j.dump();
         } else if (cmd == 2) {
            text = "cmd2";
         } else if (cmd == 3) {
            text = "cmd3";
         } else {
            std::cout << "Invalid command." << std::endl;
            cmd = -1;
         }
      }

      auto handler = [p = shared_from_this()](auto ec, auto res)
      { p->on_write(ec, res); };

      // Send the message
      ws.async_write(
         boost::asio::buffer(text),
         handler);
   }

   // Start the asynchronous operation
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

      auto handler = [p = shared_from_this()]()
      { p->prompt_for_msg(); };
      
      boost::asio::post(resolver.get_executor(), handler);
   }

   void on_write( boost::system::error_code ec
                , std::size_t bytes_transferred)
   {
      boost::ignore_unused(bytes_transferred);

      if (ec)
         return fail(ec, "write");

      auto handler = [p = shared_from_this()](auto ec, auto res)
      { p->on_read(ec, res); };

      // Read a message into our buffer
      ws.async_read(buffer, handler);
   }

   void on_read( boost::system::error_code ec
               , std::size_t bytes_transferred)
   {
      boost::ignore_unused(bytes_transferred);

      if (ec)
         return fail(ec, "read");

      auto handler = [p = shared_from_this()](auto ec)
      { p->on_close(ec); };

      // Close the WebSocket connection
      ws.async_close(websocket::close_code::normal, handler);
   }

   void on_close(boost::system::error_code ec)
   {
      if (ec)
         return fail(ec, "close");

      // If we get here then the connection is closed gracefully

      // The buffers() function helps print a ConstBufferSequence
      std::cout << boost::beast::buffers(buffer.data()) << std::endl;
   }
};

int main()
{
   boost::asio::io_context ioc;

   std::make_shared<session>(ioc)->run();

   ioc.run();

   return EXIT_SUCCESS;
}


#include "client_session.hpp"
#include "group.hpp"

namespace websocket = boost::beast::websocket;

namespace 
{

void fail(boost::system::error_code ec, char const* what)
{
   std::cerr << what << ": " << ec.message() << "\n";
}

} // Anonymous.

void client_session::write(std::string msg)
{
   text = std::move(msg);
   auto handler = [p = shared_from_this()](auto ec, auto res)
   { p->on_write(ec, res); };

   ws.async_write(boost::asio::buffer(text), handler);
}

void client_session::close()
{
   auto handler = [p = shared_from_this()](auto ec)
   { p->on_close(ec); };

   ws.async_close(websocket::close_code::normal, handler);
}

void client_session::on_resolve( boost::system::error_code ec
                               , tcp::resolver::results_type results)
{
   if (ec)
      return fail(ec, "resolve");

   auto handler = std::bind(
            &client_session::on_connect,
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

void client_session::on_connect(boost::system::error_code ec)
{
   if (ec)
      return fail(ec, "connect");

   auto handler = [p = shared_from_this()](auto ec)
   { p->on_handshake(ec); };

   // Perform the websocket handshake
   ws.async_handshake(host, "/", handler);
}

void client_session::on_handshake(boost::system::error_code ec)
{
   if (ec)
      return fail(ec, "handshake");

   do_read();
   login();
}

void client_session::do_read()
{
   auto handler = [p = shared_from_this()](auto ec, auto res)
   { p->on_read(ec, res); };

   ws.async_read(buffer, handler);
}

void client_session::on_write( boost::system::error_code ec
                             , std::size_t bytes_transferred)
{
   boost::ignore_unused(bytes_transferred);

   if (ec)
      return fail(ec, "write");
}

void client_session::on_read( boost::system::error_code ec
                            , std::size_t bytes_transferred)
{
   try {
      boost::ignore_unused(bytes_transferred);

      if (ec) {
         buffer.consume(buffer.size());

         // Insert timer here.
         do_read();
         return fail(ec, "read");
      }

      json j;
      std::stringstream ss;
      ss << boost::beast::buffers(buffer.data());
      ss >> j;
      buffer.consume(buffer.size());
      //auto str = ss.str();
      //std::cout << str << std::endl;

      auto cmd = j["cmd"].get<std::string>();

      if (cmd == "login_ack") {
         login_ack_handler(std::move(j));
      } else if (cmd == "create_group_ack") {
         create_group_ack_handler(j);
      } else if (cmd == "join_group_ack") {
         join_group_ack_handler(j);
      } else if (cmd == "message") {
         message_handler(j);
      } else {
         std::cout << "Unknown command." << std::endl;
      }

   } catch (std::exception const& e) {
      std::cerr << "Error: " << e.what() << std::endl;
   }

   do_read();
}

void client_session::on_close(boost::system::error_code ec)
{
   if (ec)
      return fail(ec, "close");

   std::cout << "Connection closed gracefully"
             << std::endl;
   work.reset();
}

void client_session::send_msg(std::string msg)
{
   auto handler = [p = shared_from_this(), msg = std::move(msg)]()
   { p->write(std::move(msg)); };

   boost::asio::post(resolver.get_executor(), handler);
}

client_session::client_session( boost::asio::io_context& ioc
                              , std::string tel_)
: resolver(ioc)
, ws(ioc)
, work(boost::asio::make_work_guard(ioc))
, tel(std::move(tel_))
{ }

void client_session::send_group_msg(std::string msg)
{
   json j;
   j["cmd"] = "send_group_msg";
   j["from"] = id;
   j["to"] = 0;
   j["msg"] = msg;

   send_msg(j.dump());
}

void client_session::send_user_msg(std::string msg)
{
   json j;
   j["cmd"] = "send_user_msg";
   j["from"] = id;
   j["to"] = 0;
   j["msg"] = msg;

   send_msg(j.dump());
}

void client_session::exit()
{
   auto handler = [p = shared_from_this()]()
   { p->close(); };

   boost::asio::post(resolver.get_executor(), handler);
}

void client_session::run()
{
   auto handler = [p = shared_from_this()](auto ec, auto res)
   { p->on_resolve(ec, res); };

   // Look up the domain name
   resolver.async_resolve(host, port, handler);
}

void client_session::login()
{
   json j;
   j["cmd"] = "login";
   j["name"] = "Marcelo Zimbres";
   j["tel"] = tel;

   send_msg(j.dump());
}

void client_session::create_group()
{
   json j;
   j["cmd"] = "create_group";
   j["from"] = id;

   group_info info { {"Repasse de automoveis Sao Paulo"}
                   , {"Destinado a pessoas fisicas e juridicas."}};

   j["info"] = info;

   send_msg(j.dump());
}

void client_session::join_group()
{
   json j;
   j["cmd"] = "join_group";
   j["from"] = id;
   j["group_idx"] = 0;

   send_msg(j.dump());
}

void client_session::login_ack_handler(json j)
{
   auto res = j["result"].get<std::string>();
   if (res == "ok") {
      id = j["user_idx"].get<int>();
      std::cout << "Id: " << id << std::endl;
      return;
   }

   std::cout << "Login failed. " << id << std::endl;
}

void client_session::create_group_ack_handler(json j)
{
   auto res = j["result"].get<std::string>();
   if (res == "ok") {
      auto group_idx = j["group_idx"].get<int>();
      groups.push_back(group_idx);
      std::cout << "Adding group: " << group_idx << std::endl;
      return;
   }

   std::cout << "Create group failed. " << id << std::endl;
}

void client_session::join_group_ack_handler(json j)
{
   auto res = j["result"].get<std::string>();
   if (res == "ok") {
      std::cout << "Joining group successful" << std::endl;
      std::cout << j << std::endl;
      return;
   }

   std::cout << "Create group failed. " << id << std::endl;
}

void client_session::message_handler(json j)
{
   auto msg = j["message"].get<std::string>();
   std::cout << msg << std::endl;
}


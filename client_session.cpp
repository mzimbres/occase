#include "client_session.hpp"
#include "group.hpp"

#include <chrono>

namespace websocket = boost::beast::websocket;

namespace 
{

void fail(boost::system::error_code ec, char const* what)
{
   std::cerr << what << ": " << ec.message() << "\n";
}

} // Anonymous.

client_session::client_session( boost::asio::io_context& ioc
                              , client_options op_)
: resolver(ioc)
, timer(ioc)
, ws(ioc)
, work(boost::asio::make_work_guard(ioc))
, op(std::move(op_))
{ }

void client_session::write(std::string msg)
{
   text = std::move(msg);
   auto handler = [p = shared_from_this()](auto ec, auto res)
   { p->on_write(ec, res); };

   ws.async_write(boost::asio::buffer(text), handler);
}

void client_session::async_close()
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

   async_connect(results);
}

void client_session::async_connect(tcp::resolver::results_type results)
{
   std::cout  << "Trying to connect." << std::endl;
   auto handler = [results, p = shared_from_this()](auto ec, auto Iterator)
   { p->on_connect(ec, results); };

   boost::asio::async_connect(ws.next_layer(), results.begin(),
      results.end(), handler);
}

void
client_session::on_connect( boost::system::error_code ec
                          , tcp::resolver::results_type results)
{
   if (ec) {
      timer.expires_after(std::chrono::seconds{1});

      auto handler = [results, p = shared_from_this()](auto ec)
      { p->async_connect(results); };

      timer.async_wait(handler);
      return;
   }

   auto handler = [p = shared_from_this(), results](auto ec)
   { p->on_handshake(ec, results); };

   // Perform the websocket handshake
   ws.async_handshake(op.host, "/", handler);
}

void
client_session::on_handshake( boost::system::error_code ec
                            , tcp::resolver::results_type results)
{
   if (ec)
      return fail(ec, "handshake");

   do_read(results);
   login();
}

void client_session::do_read(tcp::resolver::results_type results)
{
   auto handler = [p = shared_from_this(), results](auto ec, auto res)
   { p->on_read(ec, res, results); };

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
                            , std::size_t bytes_transferred
                            , tcp::resolver::results_type results)
{
   try {
      boost::ignore_unused(bytes_transferred);

      if (ec) {
         fail(ec, "read");
         buffer.consume(buffer.size());
         std::cout << "Connection lost, trying to reconnect." << std::endl;

         timer.expires_after(std::chrono::seconds{1});

         auto handler = [results, p = shared_from_this()](auto ec)
         { p->async_connect(results); };

         timer.async_wait(handler);
         return;
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

   do_read(results);
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
   { p->async_close(); };

   boost::asio::post(resolver.get_executor(), handler);
}

void client_session::run()
{
   auto handler = [p = shared_from_this()](auto ec, auto res)
   { p->on_resolve(ec, res); };

   // Look up the domain name
   resolver.async_resolve(op.host, op.port, handler);
}

void client_session::login()
{
   json j;
   j["cmd"] = "login";
   j["name"] = "Marcelo Zimbres";
   j["tel"] = op.tel;

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
   if (res == "fail") {
      std::cout << "Login failed. " << id << std::endl;
      return;
   }

   id = j["user_idx"].get<int>();
   std::cout << "Login successfull with Id: " << id << std::endl;

   if (op.interative)
      return;
   
   // We are not in an interative session, so we can proceed and
   // perform some actions.
   create_group();
}

void client_session::create_group_ack_handler(json j)
{
   --op.create_n_groups;
   auto res = j["result"].get<std::string>();
   if (res == "fail") {
      std::cout << "Create group failed. " << id << std::endl;
      return;
   }

   auto group_idx = j["group_idx"].get<int>();
   groups.insert(group_idx);
   std::cout << "Create groups successfull with id: "
             << group_idx << std::endl;

   if (op.interative)
      return;

   if (op.create_n_groups > 0)
      create_group();
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


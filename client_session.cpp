#include "client_session.hpp"
#include "group.hpp"

#include <chrono>

   //timer.expires_after(op.interval);
   //auto handler = [p = shared_from_this()](auto ec)
   //{ p->create_group(); };
   //timer.async_wait(handler);

namespace websocket = boost::beast::websocket;

namespace 
{

void fail(boost::system::error_code ec, char const* what)
{
   std::cerr << what << ": " << ec.message() << "\n";
}

} // Anonymous.

void client_session::on_read( boost::system::error_code ec
                            , std::size_t bytes_transferred
                            , tcp::resolver::results_type results)
{
   try {
      boost::ignore_unused(bytes_transferred);

      if (ec) {
         fail(ec, "read");
         buffer.consume(buffer.size());
         //std::cout << "Connection lost, trying to reconnect." << std::endl;

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
      auto str = ss.str();
      std::cout << "Received: " << str << std::endl;

      auto r = mgr.on_message(j, shared_from_this());
      if (r == -1) {
         std::cerr << "Server error. Please fix." << std::endl;
         return;
      }

   } catch (std::exception const& e) {
      std::cerr << "Server error. Please fix." << std::endl;
      std::cerr << "Error: " << e.what() << std::endl;
   }

   do_read(results);
}

client_session::client_session( boost::asio::io_context& ioc
                              , client_options op_
                              , client_mgr& m)
: resolver(ioc)
, timer(ioc)
, ws(ioc)
, work(boost::asio::make_work_guard(ioc))
, op(std::move(op_))
, mgr(m)
{ }

void client_session::write(std::string msg)
{
   //std::cout << "Sending: " << msg << std::endl;
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
   //std::cout  << "Trying to connect." << std::endl;
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

   //std::cout << "Connection stablished." << std::endl;

   auto handler = [p = shared_from_this(), results](auto ec)
   { p->on_handshake(ec, results); };

   // Perform the websocket handshake
   ws.async_handshake(op.host, "/", handler);
}

void
client_session::on_handshake( boost::system::error_code ec
                            , tcp::resolver::results_type results)
{
   //std::cout << "on_handshake" << std::endl;
   if (ec)
      return fail(ec, "handshake");

   // This function must be called before we begin to write commands
   // so that we can receive a dropped connection on the server.
   do_read(results);

   // We still have no way to use the return value here. Think of a
   // solution.
   mgr.on_handshake(shared_from_this());
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

   mgr.on_write(shared_from_this());

   if (ec)
      return fail(ec, "write");
}

void client_session::on_close(boost::system::error_code ec)
{
   if (ec)
      return fail(ec, "close");

   std::cout << "Connection closed gracefully"
             << std::endl;
   work.reset();
}

void client_session::run()
{
   auto handler = [p = shared_from_this()](auto ec, auto res)
   { p->on_resolve(ec, res); };

   // Look up the domain name
   resolver.async_resolve(op.host, op.port, handler);
}

//void client_session::prompt_login()
//{
//   auto handler = [p = shared_from_this()]() { p->login(); };
//
//   boost::asio::post(resolver.get_executor(), handler);
//}
//
//void client_session::prompt_create_group()
//{
//   auto handler = [p = shared_from_this()]()
//   { p->create_group(); };
//
//   boost::asio::post(resolver.get_executor(), handler);
//}
//
//void client_session::prompt_join_group()
//{
//   auto handler = [p = shared_from_this()]()
//   { p->join_group(); };
//
//   boost::asio::post(resolver.get_executor(), handler);
//}
//
//void client_session::prompt_send_group_msg()
//{
//   auto handler = [p = shared_from_this()]()
//   { p->send_group_msg(); };
//
//   boost::asio::post(resolver.get_executor(), handler);
//}
//
//void client_session::prompt_send_user_msg()
//{
//   auto handler = [p = shared_from_this()]()
//   { p->send_user_msg(); };
//
//   boost::asio::post(resolver.get_executor(), handler);
//}
//
//void client_session::prompt_close()
//{
//   auto handler = [p = shared_from_this()]()
//   { p->async_close(); };
//
//   boost::asio::post(resolver.get_executor(), handler);
//}

client_mgr::client_mgr(std::string tel_)
: tel(tel_)
{
}

int client_mgr::on_message(json j, std::shared_ptr<client_session> s)
{
   auto cmd = j["cmd"].get<std::string>();

   if (cmd == "login_ack") {
      return on_login_ack(std::move(j), s);
   } else if (cmd == "create_group_ack") {
      return on_create_group_ack(j, s);
   } else if (cmd == "join_group_ack") {
      return on_join_group_ack(j, s);
   } else if (cmd == "send_group_msg_ack") {
      return on_send_group_msg_ack(j, s);
   } else if (cmd == "message") {
      return on_chat_message(j, s);
   } else {
      std::cout << "Unknown command." << std::endl;
      return -1;
   }
}

int client_mgr::on_login_ack(json j, std::shared_ptr<client_session> s)
{
   auto res = j["result"].get<std::string>();
   if (res == "fail") {
      // TODO: Change longin() so that it triggers an invalid login
      // that takes us here and from here we can repost a valid login.
      std::cout << "Login failed." << std::endl;
      return 1;
   }

   bind = j["user_bind"].get<user_bind>();
   std::cout << "Login successfull with bind: \n" << bind << std::endl;

   // TODO: Before we proceed with create group we could repost a
   // login command to see how the server responds. Let us do it
   // later, this will make the code even more complicated.

   create_group(s);
   return 1;
}

int
client_mgr::on_create_group_ack(json j, std::shared_ptr<client_session> s)
{
   // TODO: Split this in ok and fail conditions below.
   if (number_of_create_groups > 0) {
      create_group(s);
   } else {
      join_group(s);
   }

   auto res = j["result"].get<std::string>();
   if (res == "ok") {
      auto gbind = j["group_bind"].get<group_bind>();
      groups.insert(gbind);

      std::cout << "Create groups successfull: \n" << gbind
                << std::endl;
      return 1;
   }
   
   if (res == "fail") {
      std::cout << "Create group failed." << std::endl;
      return 1;
   }

   return -1;
}

int client_mgr::on_chat_message(json j, std::shared_ptr<client_session>)
{
   auto msg = j["message"].get<std::string>();
   std::cout << msg << std::endl;
   return 1;
}

int
client_mgr::on_join_group_ack(json j, std::shared_ptr<client_session> s)
{
   // TODO: Split this in the fail, ok conditions below.
   if (number_of_joins > 0) {
      join_group(s);
   } else {
      send_group_msg(s);
   }

   auto res = j["result"].get<std::string>();

   if (res == "ok") {
      std::cout << "Joining group successful" << std::endl;
      auto info = j["info"].get<group_info>();
      std::cout << info << std::endl;
      return 1;
   }

   if (res == "fail") {
      std::cout << "Joining group failed." << std::endl;
      return 1;
   }

   return -1;
}

int
client_mgr::on_send_group_msg_ack(json j, std::shared_ptr<client_session> s)
{
   if (number_of_group_msgs > 0) {
      send_group_msg(s);
   }

   auto res = j["result"].get<std::string>();
   if (res == "ok") {
      std::cout << "Send group message ok." << std::endl;
      return 1;
   }

   if (res == "fail") {
      std::cout << "Send group message fail." << std::endl;
      return 1;
   }

   return -1;
}

void client_mgr::login(std::shared_ptr<client_session> s)
{
   if (number_of_logins == 4) {
      json j;
      j["cmd"] = "logrn";
      j["tel"] =tel;
      send_msg(j.dump(), s);
      --number_of_logins;
      return;
   }

   if (number_of_logins == 3) {
      json j;
      j["crd"] = "login";
      j["tel"] = tel;
      send_msg(j.dump(), s);
      --number_of_logins;
      return;
   }

   if (number_of_logins == 2) {
      json j;
      j["crd"] = "login";
      j["Teal"] = tel;
      send_msg(j.dump(), s);
      --number_of_logins;
      return;
   }

   // The valid login.
   if (number_of_logins == 1) {
      json j;
      j["cmd"] = "login";
      j["tel"] = tel;
      send_msg(j.dump(), s);
      --number_of_logins;
      return;
   }
}

void client_mgr::create_group(std::shared_ptr<client_session> s)
{
   //std::cout << "Create group: " << number_of_create_groups << std::endl;

   // Should be accepted by the server.
   if (number_of_create_groups == 8) {
      json j;
      j["cmd"] = "create_group";
      j["from"] = bind;
      j["info"] = group_info { {"Repasse"}, {"Carros."}};
      send_msg(j.dump(), s);
      if (--number_of_valid_create_groups == 0)
         --number_of_create_groups;
      //std::cout << "Just send a valid create group: " << j << std::endl;
      return;
   }

   if (number_of_create_groups == 7) {
      json j;
      j["cmud"] = "create_group";
      j["from"] = bind;
      j["info"] = group_info { {"Repasse"}, {"Carros."}};
      send_msg(j.dump(), s);
      --number_of_create_groups;
      return;
   }

   if (number_of_create_groups == 6) {
      json j;
      j["cmd"] = "craeate_group";
      j["from"] = bind;
      j["info"] = group_info { {"Repasse"}, {"Carros."}};
      send_msg(j.dump(), s);
      --number_of_create_groups;
      return;
   }

   if (number_of_create_groups == 5) {
      json j;
      j["cmd"] = "create_group";
      j["froim"] = bind;
      j["info"] = group_info { {"Repasse"}, {"Carros."}};
      send_msg(j.dump(), s);
      --number_of_create_groups;
      return;
   }

   if (number_of_create_groups == 4) {
      json j;
      j["cmd"] = "create_group";
      auto tmp = bind;
      ++bind.index;
      j["from"] = bind;
      j["info"] = group_info { {"Repasse"}, {"Carros."}};
      send_msg(j.dump(), s);
      --number_of_create_groups;
      return;
   }

   if (number_of_create_groups == 3) {
      json j;
      j["cmd"] = "create_group";
      j["from"] = bind;
      j["inafo"] = group_info { {"Repasse"}, {"Carros."}};
      send_msg(j.dump(), s);
      --number_of_create_groups;
      return;
   }

   if (number_of_create_groups == 2) {
      json j;
      j["cmd"] = "create_group";
      j["from"] = bind;
      j["info"] = "aaaaa";
      send_msg(j.dump(), s);
      --number_of_create_groups;
      return;
   }

   if (number_of_create_groups == 1) {
      send_msg("alkdshfkjds", s);
      --number_of_create_groups;
      return;
   }
}

void client_mgr::join_group(std::shared_ptr<client_session> s)
{
   if (number_of_joins == 4) {
      json j;
      j["cmd"] = "join_group";
      j["from"] = bind;
      j["group_iid"] = group_bind {"wwr", number_of_joins};
      send_msg(j.dump(), s);
      --number_of_joins;
      return;
   }

   if (number_of_joins == 3) {
      json j;
      j["cmd"] = "join_group";
      auto tmp = bind;
      ++tmp.index;
      j["from"] = tmp;
      j["group_bind"] = group_bind {"criatura", number_of_joins};
      send_msg(j.dump(), s);
      --number_of_joins;
      return;
   }

   if (number_of_joins == 2) {
      json j;
      j["cmd"] = "joiin_group";
      j["from"] = bind;
      j["group_bind"] = group_bind {"criatura", number_of_joins};
      send_msg(j.dump(), s);
      --number_of_joins;
      return;
   }

   if (number_of_joins == 1) {
      json j;
      j["cmd"] = "join_group";
      j["from"] = bind;
      j["group_bind"] = group_bind {"criatura", number_of_valid_joins};
      send_msg(j.dump(), s);
      if (number_of_valid_joins-- == 0)
         --number_of_joins;
      return;
   }
}

void client_mgr::send_group_msg(std::shared_ptr<client_session> s)
{
   if (number_of_group_msgs == 5) {
      json j;
      j["cmd"] = "send_group_msg";
      auto tmp = bind;
      ++tmp.index;
      j["from"] = tmp;
      j["to"] = 0;
      j["msg"] = "Message to group";
      send_msg(j.dump(), s);
      --number_of_group_msgs;
   }

   if (number_of_group_msgs == 4) {
      json j;
      j["cmd"] = "send_group_msg";
      j["friom"] = bind;
      j["to"] = 0;
      j["msg"] = "Message to group";
      send_msg(j.dump(), s);
      --number_of_group_msgs;
   }

   if (number_of_group_msgs == 3) {
      json j;
      j["cmd"] = "send_9group_msg";
      j["from"] = bind;
      j["to"] = 0;
      j["msg"] = "Message to group";
      send_msg(j.dump(), s);
      --number_of_group_msgs;
   }

   if (number_of_group_msgs == 2) {
      json j;
      j["cm2d"] = "send_group_msg";
      j["from"] = bind;
      j["to"] = 0;
      j["msg"] = "Message to group";
      send_msg(j.dump(), s);
      --number_of_group_msgs;
   }

   if (number_of_group_msgs == 1) {
      json j;
      j["cmd"] = "send_group_msg";
      j["from"] = bind;
      j["to"] = number_of_valid_group_msgs;
      j["msg"] = "Message to group";
      send_msg(j.dump(), s);
      if (number_of_valid_group_msgs-- == 0)
         --number_of_group_msgs;
   }
}

void client_mgr::send_user_msg(std::shared_ptr<client_session> s)
{
   json j;
   j["cmd"] = "send_user_msg";
   j["from"] = bind;
   j["to"] = 0;
   j["msg"] = "Mensagem ao usuario.";

   send_msg(j.dump(), s);
}

void client_mgr::send_msg(std::string msg, std::shared_ptr<client_session> s)
{
   auto is_empty = std::empty(msg_queue);
   msg_queue.push(std::move(msg));

   if (is_empty)
      s->write(msg_queue.front());
}

void client_mgr::on_write(std::shared_ptr<client_session> s)
{
   //std::cout << "on_write" << std::endl;

   msg_queue.pop();
   if (msg_queue.empty())
      return; // No more message to send to the client.

   s->write(msg_queue.front());
}

int client_mgr::on_handshake(std::shared_ptr<client_session> s)
{
   if (number_of_logins > 0) {
      login(s);
      --number_of_dropped_logins;
      return 1;
   }

   if (number_of_dropped_logins != 0) {
      std::cout << "Error: number_of_dropped_logins != 0" << std::endl;
      return -1;
   }

   if (number_of_create_groups > 0) {
      create_group(s);
      --number_of_dropped_create_groups;
      return 1;
   }

   if (number_of_dropped_create_groups != 0) {
      std::cout << "Error: " << number_of_dropped_create_groups << " != 0"
                << std::endl;
      return -1;
   }

   if (number_of_joins > 0) {
      join_group(s);
      return 1;
   }

   if (number_of_group_msgs > 0) {
      send_group_msg(s);
      return 1;
   }

   return 1;
}


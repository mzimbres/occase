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
      //std::cout << "Received: " << str << std::endl;

      auto cmd = j["cmd"].get<std::string>();

      if (cmd == "login_ack") {
         on_login_ack(std::move(j));
      } else if (cmd == "create_group_ack") {
         on_create_group_ack(j);
      } else if (cmd == "join_group_ack") {
         on_join_group_ack(j);
      } else if (cmd == "send_group_msg_ack") {
         on_send_group_msg_ack(j);
      } else if (cmd == "message") {
         on_message(j);
      } else {
         std::cout << "Unknown command." << std::endl;
      }

   } catch (std::exception const& e) {
      std::cerr << "Error: " << e.what() << std::endl;
   }

   do_read(results);
}

void client_session::on_message(json j)
{
   auto msg = j["message"].get<std::string>();
   std::cout << msg << std::endl;
}

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

   do_read(results);

   if (number_of_logins > 0) {
      login();
      return;
   }

   if (number_of_create_groups > 0) {
      create_group();
      return;
   }

   if (number_of_joins > 0) {
      join_group();
      return;
   }

   if (number_of_group_msgs > 0) {
      send_group_msg();
      return;
   }
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
   //std::cout << "on_write" << std::endl;

   boost::ignore_unused(bytes_transferred);

   msg_queue.pop();
   if (msg_queue.empty())
      return; // No more message to send to the client.

   write(msg_queue.front());

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

void client_session::send_msg(std::string msg)
{
   auto is_empty = std::empty(msg_queue);
   msg_queue.push(std::move(msg));

   if (is_empty)
      write(msg_queue.front());
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
   if (number_of_logins == 5) {
      json j;
      j["cmd"] = "logrn";
      j["name"] = "Marcelo Zimbres";
      j["tel"] = op.tel;
      send_msg(j.dump());
      --number_of_logins;
      return;
   }

   if (number_of_logins == 4) {
      json j;
      j["crd"] = "login";
      j["name"] = "Marcelo Zimbres";
      j["tel"] = op.tel;
      send_msg(j.dump());
      --number_of_logins;
      return;
   }

   if (number_of_logins == 3) {
      json j;
      j["crd"] = "login";
      j["nume"] = "Marcelo Zimbres";
      j["tel"] = op.tel;
      send_msg(j.dump());
      --number_of_logins;
      return;
   }

   if (number_of_logins == 2) {
      json j;
      j["crd"] = "login";
      j["nuMe"] = "Marcelo Zimbres";
      j["Teal"] = op.tel;
      send_msg(j.dump());
      --number_of_logins;
      return;
   }

   // The correct login.
   if (number_of_logins == 1) {
      json j;
      j["cmd"] = "login";
      j["name"] = "Marcelo Zimbres";
      j["tel"] = op.tel;
      send_msg(j.dump());
      --number_of_logins;
      return;
   }
}

void client_session::prompt_login()
{
   auto handler = [p = shared_from_this()]() { p->login(); };

   boost::asio::post(resolver.get_executor(), handler);
}

void client_session::create_group()
{
   //std::cout << "Create group: " << number_of_create_groups << std::endl;
   if (number_of_create_groups == 8) {
      json j;
      j["cmd"] = "create_group";
      j["from"] = id;
      group_info info { {"Repasse"}, {"Carros."}};
      j["info"] = info;
      send_msg({j.dump()});
      if (--number_of_valid_create_groups == 0)
         --number_of_create_groups;
      //std::cout << "Just send a valid create group: " << j << std::endl;
      return;
   }

   if (number_of_create_groups == 7) {
      json j;
      j["cmud"] = "create_group";
      j["from"] = id;
      group_info info { {"Repasse"}, {"Carros."}};
      j["info"] = info;
      send_msg({j.dump()});
      --number_of_create_groups;
      return;
   }

   if (number_of_create_groups == 6) {
      json j;
      j["cmd"] = "craeate_group";
      j["from"] = id;
      group_info info { {"Repasse"}, {"Carros."}};
      j["info"] = info;
      send_msg({j.dump()});
      --number_of_create_groups;
      return;
   }

   if (number_of_create_groups == 5) {
      json j;
      j["cmd"] = "create_group";
      j["froim"] = id;
      group_info info { {"Repasse"}, {"Carros."}};
      j["info"] = info;
      send_msg({j.dump()});
      --number_of_create_groups;
      return;
   }

   if (number_of_create_groups == 4) {
      json j;
      j["cmd"] = "create_group";
      j["from"] = id + 1;
      group_info info { {"Repasse"}, {"Carros."}};
      j["info"] = info;
      send_msg({j.dump()});
      --number_of_create_groups;
      return;
   }

   if (number_of_create_groups == 3) {
      json j;
      j["cmd"] = "create_group";
      j["from"] = id;
      group_info info { {"Repasse"}, {"Carros."}};
      j["inafo"] = info;
      send_msg({j.dump()});
      --number_of_create_groups;
      return;
   }

   if (number_of_create_groups == 2) {
      json j;
      j["cmd"] = "create_group";
      j["from"] = id;
      j["info"] = "aaaaa";
      send_msg({j.dump()});
      --number_of_create_groups;
      return;
   }

   if (number_of_create_groups == 1) {
      send_msg("alkdshfkjds");
      --number_of_create_groups;
      return;
   }
}

void client_session::prompt_create_group()
{
   auto handler = [p = shared_from_this()]()
   { p->create_group(); };

   boost::asio::post(resolver.get_executor(), handler);
}

void client_session::join_group()
{
   if (number_of_joins == 4) {
      json j;
      j["cmd"] = "join_group";
      j["from"] = id; // Should cause error.
      j["group_iid"] = number_of_joins;
      send_msg({j.dump()});
      --number_of_joins;
      return;
   }

   if (number_of_joins == 3) {
      json j;
      j["cmd"] = "join_group";
      j["from"] = id + 1; // Should cause error.
      j["group_id"] = number_of_joins;
      send_msg({j.dump()});
      --number_of_joins;
      return;
   }

   if (number_of_joins == 2) {
      json j;
      j["cmd"] = "joiin_group";
      j["from"] = id;
      j["group_id"] = number_of_joins;
      send_msg({j.dump()});
      --number_of_joins;
      return;
   }

   if (number_of_joins == 1) {
      json j;
      j["cmd"] = "join_group";
      j["from"] = id;
      j["group_id"] = number_of_valid_joins;
      send_msg({j.dump()});
      if (number_of_valid_joins-- == 0)
         --number_of_joins;
      return;
   }
}

void client_session::prompt_join_group()
{
   auto handler = [p = shared_from_this()]()
   { p->join_group(); };

   boost::asio::post(resolver.get_executor(), handler);
}

void client_session::send_group_msg()
{
   if (number_of_group_msgs == 5) {
      json j;
      j["cmd"] = "send_group_msg";
      j["from"] = id + 1;
      j["to"] = 0;
      j["msg"] = "Message to group";
      send_msg(j.dump());
      --number_of_group_msgs;
   }

   if (number_of_group_msgs == 4) {
      json j;
      j["cmd"] = "send_group_msg";
      j["friom"] = id;
      j["to"] = 0;
      j["msg"] = "Message to group";
      send_msg(j.dump());
      --number_of_group_msgs;
   }

   if (number_of_group_msgs == 3) {
      json j;
      j["cmd"] = "send_9group_msg";
      j["from"] = id;
      j["to"] = 0;
      j["msg"] = "Message to group";
      send_msg(j.dump());
      --number_of_group_msgs;
   }

   if (number_of_group_msgs == 2) {
      json j;
      j["cm2d"] = "send_group_msg";
      j["from"] = id;
      j["to"] = 0;
      j["msg"] = "Message to group";
      send_msg(j.dump());
      --number_of_group_msgs;
   }

   if (number_of_group_msgs == 1) {
      json j;
      j["cmd"] = "send_group_msg";
      j["from"] = id;
      j["to"] = number_of_valid_group_msgs;
      j["msg"] = "Message to group";
      send_msg(j.dump());
      if (number_of_valid_group_msgs-- == 0)
         --number_of_group_msgs;
   }
}

void client_session::prompt_send_group_msg()
{
   auto handler = [p = shared_from_this()]()
   { p->send_group_msg(); };

   boost::asio::post(resolver.get_executor(), handler);
}

void client_session::send_user_msg()
{
   json j;
   j["cmd"] = "send_user_msg";
   j["from"] = id;
   j["to"] = 0;
   j["msg"] = "Mensagem ao usuario.";

   send_msg({j.dump()});
}

void client_session::prompt_send_user_msg()
{
   auto handler = [p = shared_from_this()]()
   { p->send_user_msg(); };

   boost::asio::post(resolver.get_executor(), handler);
}

void client_session::prompt_close()
{
   auto handler = [p = shared_from_this()]()
   { p->async_close(); };

   boost::asio::post(resolver.get_executor(), handler);
}

void client_session::on_login_ack(json j)
{
   if (!op.interative) {
      timer.expires_after(op.interval);
      if (number_of_logins > 0) {
         // As it is now, this condition will not happen.
         auto handler = [p = shared_from_this()](auto ec)
         { p->login(); };

         timer.async_wait(handler);
      }
   }

   auto res = j["result"].get<std::string>();
   if (res == "fail") {
      std::cout << "Login failed. " << id << std::endl;
      return;
   }

   id = j["user_idx"].get<int>();
   std::cout << "Login successfull with Id: " << id << std::endl;

   if (!op.interative) {
      timer.expires_after(op.interval);
      auto handler = [p = shared_from_this()](auto ec)
      { p->create_group(); };

      timer.async_wait(handler);
   }
}

void client_session::on_create_group_ack(json j)
{
   if (!op.interative) {
      timer.expires_after(op.interval);
      if (number_of_create_groups > 0) {
         // This is not gonna happen. CHANGE.
         auto handler = [p = shared_from_this()](auto ec)
         { p->create_group(); };

         timer.async_wait(handler);
      } else {
         auto handler = [p = shared_from_this()](auto ec)
         { p->join_group(); };

         timer.async_wait(handler);
      }
   }

   auto res = j["result"].get<std::string>();
   if (res == "fail") {
      std::cout << "Create group failed." << std::endl;
      return;
   }

   auto group_id = j["group_id"].get<int>();
   groups.insert(group_id);

   std::cout << "Create groups successfull: " << group_id
             << std::endl;
}

void client_session::on_join_group_ack(json j)
{
   if (!op.interative) {
      timer.expires_after(op.interval);
      // Further joins that are not dropped by the server for being invalid
      // jsons could be also triggered here.
      if (number_of_joins > 0) {
         auto handler = [p = shared_from_this()](auto ec)
         { p->join_group(); };

         timer.async_wait(handler);
      } else {
         auto handler = [p = shared_from_this()](auto ec)
         { p->send_group_msg(); };

         timer.async_wait(handler);
      }
   }

   auto res = j["result"].get<std::string>();
   if (res == "fail") {
      std::cout << "Joining group failed." << std::endl;
      return;
   }

   std::cout << "Joining group successful" << std::endl;
   auto info = j["info"].get<group_info>();
   std::cout << info << std::endl;
}

void client_session::on_send_group_msg_ack(json j)
{
   if (!op.interative) {
      timer.expires_after(op.interval);
      if (number_of_group_msgs > 0) {
         auto handler = [p = shared_from_this()](auto ec)
         { p->send_group_msg(); };

         timer.async_wait(handler);
      }
   }

   auto res = j["result"].get<std::string>();
   if (res == "fail") {
      std::cout << "Send group message fail." << std::endl;
      return;
   }

   std::cout << "Send group message ok." << std::endl;

   //if (!op.interative) {
   //   timer.expires_after(op.interval);
   //   auto handler = [p = shared_from_this()](auto ec)
   //   { p->create_group(); };

   //   timer.async_wait(handler);
   //}
}


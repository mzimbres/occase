#include "client_mgr.hpp"

#include "client_session.hpp"

client_mgr::client_mgr(std::string tel_)
: tel(tel_)
{ }

int client_mgr::on_read(json j, std::shared_ptr<client_session> s)
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

int client_mgr::on_fail_read(boost::system::error_code ec)
{
   std::cerr << "client_mgr::on_fail_read: " << ec.message() << "\n";

   if (number_of_dropped_logins != 0) {
      --number_of_dropped_logins;
      return 1;
   }

   if (number_of_dropped_create_groups != 0) {
      --number_of_dropped_create_groups;
      return 1;
   }

   if (number_of_dropped_joins > 0) {
      --number_of_dropped_joins;
      return 1;
   }

   return 1;
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
      ++tmp.index;
      j["from"] = tmp;
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
      return 1;
   }

   if (number_of_dropped_logins != 0) {
      std::cout
      << "Error: client_mgr::on_handshake number_of_dropped_logins: "
      << number_of_dropped_logins <<  " != 0" << std::endl;
      return -1;
   }

   if (number_of_create_groups > 0) {
      create_group(s);
      return 1;
   }

   if (number_of_dropped_create_groups != 0) {
      std::cout
       << "Error: client_mgr::on_handshake number_of_dropped_create_groups: "
       << number_of_dropped_create_groups << " != 0" << std::endl;
      return -1;
   }

   if (number_of_joins > 0) {
      join_group(s);
      return 1;
   }

   if (number_of_dropped_joins != 0) {
      std::cout
       << "Error: client_mgr::on_handshake number_of_dropped_joins: "
       << number_of_dropped_joins << " != 0" << std::endl;
      return -1;
   }

   if (number_of_group_msgs > 0) {
      send_group_msg(s);
      return 1;
   }

   return 1;
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

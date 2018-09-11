#include "client_mgr.hpp"

#include "client_session.hpp"

client_mgr::client_mgr(std::string tel_)
: tel(tel_)
{ }

int client_mgr::on_read(json j, std::shared_ptr<client_type> s)
{
   auto cmd = j["cmd"].get<std::string>();

   if (cmd == "login_ack") {
      return on_login_ack(std::move(j), s);
   } else if (cmd == "sms_confirmation_ack") {
      return on_sms_confirmation_ack(std::move(j), s);
   } else if (cmd == "create_group_ack") {
      return on_create_group_ack(std::move(j), s);
   } else if (cmd == "join_group_ack") {
      return on_join_group_ack(std::move(j), s);
   } else if (cmd == "send_group_msg_ack") {
      return on_send_group_msg_ack(std::move(j), s);
   } else if (cmd == "message") {
      return on_chat_message(std::move(j), s);
   } else {
      std::cout << "Unknown command." << std::endl;
      std::cerr << "Server error. Please fix." << std::endl;
      return -1;
   }
}

int client_mgr::on_closed(boost::system::error_code ec)
{
   //std::cerr << "client_mgr::on_closed: " << ec.message() << "\n";

   if (number_of_dropped_create_groups > 0) {
      --number_of_dropped_create_groups;
      return 1;
   }

   if (number_of_dropped_joins > 0) {
      --number_of_dropped_joins;
      return 1;
   }

   return 1;
}

int client_mgr::on_login_ack(json j, std::shared_ptr<client_type> s)
{
   auto res = j["result"].get<std::string>();
   if (res == "ok") {
      std::cout << "login_ack: ok." << std::endl;
      ok_sms_confirmation(s);
      return 1;
   }

   // We still have not fail login server ack.
   std::cout << "SERVER ERROR: client_mgr::on_login_ack.\n"
             << "Please report." << std::endl;
   return 1;
}

int
client_mgr::on_sms_confirmation_ack(json j, std::shared_ptr<client_type> s)
{
   auto res = j["result"].get<std::string>();
   if (res == "ok") {
      bind = j["user_bind"].get<user_bind>();
      std::cout << "sms_confirmation_ack: ok."
                //<< "\n" << bind
                << std::endl;

      // Now we begin to post the create groups that will be dropped.
      dropped_create_group(s);
      return 1;
   }

   std::cout << "LOGIN ERROR: client_mgr::on_sms_confirmation_ack.\n"
             << "Please report."
             << std::endl;

   return 1;
}

int
client_mgr::on_ok_create_group_ack(json j, std::shared_ptr<client_type> s)
{
   //auto gbind = j["group_bind"].get<group_bind>();

   std::cout << "create_group: ok."
             //<< "\n" << gbind
             << std::endl;

   if (number_of_ok_create_groups-- == 0) {
      fail_create_group(s);
      return 1;
   }

   ok_create_group(s);
   return 1;
}

int
client_mgr::on_fail_create_group_ack( json j
                                    , std::shared_ptr<client_type> s)
{
   std::cout << "create_group: fail." << std::endl;

   if (number_of_fail_create_groups-- == 0) {
      ok_join_group(s);
      return 1;
   }

   fail_create_group(s);
   return 1;
}

int
client_mgr::on_create_group_ack(json j, std::shared_ptr<client_type> s)
{
   auto res = j["result"].get<std::string>();
   if (res == "ok")
      return on_ok_create_group_ack(j, s);
   
   if (res == "fail")
      return on_fail_create_group_ack(j, s);

   return -1;
}

int client_mgr::on_chat_message(json j, std::shared_ptr<client_type>)
{
   //auto msg = j["message"].get<std::string>();
   //std::cout << msg << std::endl;
   return 1;
}

int
client_mgr::on_ok_join_group_ack(json j, std::shared_ptr<client_type> s)
{
   if (number_of_ok_joins-- == 0) {
      dropped_join_group(s);
      return 1;
   }

   std::cout << "Joining group successful" << std::endl;
   auto info = j["info"].get<group_info>();
   std::cout << info << std::endl;
   ok_join_group(s);
   return 1;
}

int
client_mgr::on_join_group_ack(json j, std::shared_ptr<client_type> s)
{
   std::cout << "join_group: ok." << std::endl;

   auto res = j["result"].get<std::string>();

   if (res == "ok")
      return on_ok_join_group_ack(j, s);

   std::cout << "SERVER ERROR: Please report." << std::endl;
   return -1;
}

int
client_mgr::on_send_group_msg_ack(json j, std::shared_ptr<client_type> s)
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

void client_mgr::ok_sms_confirmation(std::shared_ptr<client_type> s)
{
   json j;
   j["cmd"] = "sms_confirmation";
   j["tel"] = tel;
   j["sms"] = "8347";
   s->send_msg(j.dump());
}

void client_mgr::ok_create_group(std::shared_ptr<client_type> s)
{
   json j;
   j["cmd"] = "create_group";
   j["from"] = bind;
   j["info"] = group_info { {"Repasse"}, {"Carros."}};
   s->send_msg(j.dump());
   //std::cout << "Just send a valid create group: " << j << std::endl;
}

void client_mgr::fail_create_group(std::shared_ptr<client_type> s)
{
   // This is a valid command but should exceed the server capacity.
   json j;
   j["cmd"] = "create_group";
   j["from"] = bind;
   j["info"] = group_info { {"Repasse"}, {"Carros."}};
   s->send_msg(j.dump());
}

void client_mgr::dropped_create_group(std::shared_ptr<client_type> s)
{
   if (number_of_dropped_create_groups == 7) {
      json j;
      j["cmud"] = "create_group";
      j["from"] = bind;
      j["info"] = group_info { {"Repasse"}, {"Carros."}};
      s->send_msg(j.dump());
      return;
   }

   if (number_of_dropped_create_groups == 6) {
      json j;
      j["cmd"] = "craeate_group";
      j["from"] = bind;
      j["info"] = group_info { {"Repasse"}, {"Carros."}};
      s->send_msg(j.dump());
      return;
   }

   if (number_of_dropped_create_groups == 5) {
      json j;
      j["cmd"] = "create_group";
      j["froim"] = bind;
      j["info"] = group_info { {"Repasse"}, {"Carros."}};
      s->send_msg(j.dump());
      return;
   }

   if (number_of_dropped_create_groups == 4) {
      json j;
      j["cmd"] = "create_group";
      auto tmp = bind;
      ++tmp.index;
      j["from"] = tmp;
      j["info"] = group_info { {"Repasse"}, {"Carros."}};
      s->send_msg(j.dump());
      return;
   }

   if (number_of_dropped_create_groups == 3) {
      json j;
      j["cmd"] = "create_group";
      j["from"] = bind;
      j["inafo"] = group_info { {"Repasse"}, {"Carros."}};
      s->send_msg(j.dump());
      return;
   }

   if (number_of_dropped_create_groups == 2) {
      json j;
      j["cmd"] = "create_group";
      j["from"] = bind;
      j["info"] = "aaaaa";
      s->send_msg(j.dump());
      return;
   }

   if (number_of_dropped_create_groups == 1) {
      s->send_msg("alkdshfkjds");
      return;
   }
}

void client_mgr::ok_join_group(std::shared_ptr<client_type> s)
{
   json j;
   j["cmd"] = "join_group";
   j["from"] = bind;
   //j["group_bind"] = group_bind {"criatura"};
   s->send_msg(j.dump());
}

void client_mgr::dropped_join_group(std::shared_ptr<client_type> s)
{
   if (number_of_dropped_joins == 3) {
      json j;
      j["cmd"] = "join_group";
      j["from"] = bind;
      //j["group_iid"] = group_bind {"wwr"};
      s->send_msg(j.dump());
      return;
   }

   if (number_of_dropped_joins == 2) {
      json j;
      j["cmd"] = "join_group";
      auto tmp = bind;
      ++tmp.index;
      j["from"] = tmp;
      //j["group_bind"] = group_bind {"criatura"};
      s->send_msg(j.dump());
      return;
   }

   if (number_of_dropped_joins == 1) {
      json j;
      j["cmd"] = "joiin_group";
      j["from"] = bind;
      //j["group_bind"] = group_bind {"criatura"};
      s->send_msg(j.dump());
      return;
   }
}

void client_mgr::send_group_msg(std::shared_ptr<client_type> s)
{
   if (number_of_group_msgs == 5) {
      json j;
      j["cmd"] = "send_group_msg";
      auto tmp = bind;
      ++tmp.index;
      j["from"] = tmp;
      j["to"] = 0;
      j["msg"] = "Message to group";
      s->send_msg(j.dump());
      --number_of_group_msgs;
   }

   if (number_of_group_msgs == 4) {
      json j;
      j["cmd"] = "send_group_msg";
      j["friom"] = bind;
      j["to"] = 0;
      j["msg"] = "Message to group";
      s->send_msg(j.dump());
      --number_of_group_msgs;
   }

   if (number_of_group_msgs == 3) {
      json j;
      j["cmd"] = "send_9group_msg";
      j["from"] = bind;
      j["to"] = 0;
      j["msg"] = "Message to group";
      s->send_msg(j.dump());
      --number_of_group_msgs;
   }

   if (number_of_group_msgs == 2) {
      json j;
      j["cm2d"] = "send_group_msg";
      j["from"] = bind;
      j["to"] = 0;
      j["msg"] = "Message to group";
      s->send_msg(j.dump());
      --number_of_group_msgs;
   }

   if (number_of_group_msgs == 1) {
      json j;
      j["cmd"] = "send_group_msg";
      j["from"] = bind;
      j["to"] = number_of_valid_group_msgs;
      j["msg"] = "Message to group";
      s->send_msg(j.dump());
      if (number_of_valid_group_msgs-- == 0)
         --number_of_group_msgs;
   }
}

void client_mgr::send_user_msg(std::shared_ptr<client_type> s)
{
   json j;
   j["cmd"] = "send_user_msg";
   j["from"] = bind;
   j["to"] = 0;
   j["msg"] = "Mensagem ao usuario.";

   s->send_msg(j.dump());
}

int client_mgr::on_handshake(std::shared_ptr<client_type> s)
{
   if (!login) {
      json j;
      j["cmd"] = "login";
      j["tel"] = tel;
      s->send_msg(j.dump());
      login = true;
   }

   if (number_of_dropped_create_groups > 0) {
      dropped_create_group(s);
      return 1;
   }

   if (number_of_dropped_create_groups == 0) {
      std::cout << "number_of_dropped_create_groups: Ok" << std::endl;
      --number_of_dropped_create_groups;
   }

   if (number_of_ok_create_groups > 0) {
      ok_create_group(s);
      return 1;
   }

   if (number_of_ok_create_groups == 0) {
      std::cout << "number_of_ok_create_groups: Ok" << std::endl;
      --number_of_ok_create_groups;
   }

   if (number_of_dropped_joins > 0) {
      dropped_join_group(s);
      return 1;
   }

   if (number_of_group_msgs > 0) {
      send_group_msg(s);
      return 1;
   }

   return 1;
}


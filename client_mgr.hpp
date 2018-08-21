#pragma once

#include <set>
#include <queue>
#include <string>
#include <memory>

#include "config.hpp"
#include "json_utils.hpp"

class client_session;

class client_mgr {
private:
   std::set<group_bind> groups;
   std::queue<std::string> msg_queue;
   std::string tel;

   int number_of_logins = 4;
   int number_of_dropped_logins = number_of_logins;

   int number_of_valid_create_groups = 10;
   int number_of_create_groups = 8;
   int number_of_dropped_create_groups = number_of_create_groups - 2;

   int number_of_valid_joins = 10;
   int number_of_joins = 4;

   int number_of_valid_group_msgs = 10;
   int number_of_group_msgs = 4;

   // Functions called when new message arrives. If any of these
   // functions return -1 it means the server returned something wrong
   // and we should make the test fail.
   int on_login_ack(json j, std::shared_ptr<client_session> s);
   int on_create_group_ack(json j, std::shared_ptr<client_session> s);
   int on_chat_message(json j, std::shared_ptr<client_session> s);
   int on_join_group_ack(json j, std::shared_ptr<client_session> s);
   int on_send_group_msg_ack(json j, std::shared_ptr<client_session> s);

   // Functions that create some random commands and sends them to the
   // server.
   void login(std::shared_ptr<client_session> s);
   void create_group(std::shared_ptr<client_session> s);
   void join_group(std::shared_ptr<client_session> s);
   void send_group_msg(std::shared_ptr<client_session> s);
   void send_user_msg(std::shared_ptr<client_session> s);

   void send_msg(std::string msg, std::shared_ptr<client_session> s);

public:
   client_mgr(std::string tel_);
   user_bind bind;
   int on_read(json j, std::shared_ptr<client_session> s);
   int on_fail_read(boost::system::error_code ec);
   void on_write(std::shared_ptr<client_session> s);
   int on_handshake(std::shared_ptr<client_session> s);
   //void prompt_login();
   //void prompt_create_group();
   //void prompt_join_group();
   //void prompt_send_group_msg();
   //void prompt_send_user_msg();
   //void prompt_close();
};


#pragma once

#include <set>
#include <queue>
#include <string>
#include <memory>

#include "config.hpp"
#include "json_utils.hpp"

template <class Mgr>
class client_session;

class client_mgr {
private:
   using client_type = client_session<client_mgr>;
   std::set<group_bind> groups;
   std::queue<std::string> msg_queue;
   std::string tel;

   // login
   int number_of_ok_logins = 1;
   int number_of_dropped_logins = 3;

   void ok_login(std::shared_ptr<client_type> s);
   void dropped_login(std::shared_ptr<client_type> s);
   int on_login_ack(json j, std::shared_ptr<client_type> s);
   int on_ok_login_ack(json j, std::shared_ptr<client_type> s);

   // sms
   int number_of_ok_sms = 1;

   void ok_sms_confirmation(std::shared_ptr<client_type> s);
   int on_sms_confirmation_ack(json j, std::shared_ptr<client_type> s);
   int on_ok_sms_confirmation_ack(json j, std::shared_ptr<client_type> s);

   // create_group
   int number_of_ok_create_groups = 10;
   int number_of_fail_create_groups = 10;
   int number_of_dropped_create_groups = 7;

   int on_create_group_ack(json j, std::shared_ptr<client_type> s);
   int on_ok_create_group_ack(json j, std::shared_ptr<client_type> s);
   int on_fail_create_group_ack(json j, std::shared_ptr<client_type> s);

   void ok_create_group(std::shared_ptr<client_type> s);
   void fail_create_group(std::shared_ptr<client_type> s);
   void dropped_create_group(std::shared_ptr<client_type> s);

   // join_group
   int number_of_ok_joins = 10;
   int number_of_dropped_joins = 3;

   void ok_join_group(std::shared_ptr<client_type> s);
   void dropped_join_group(std::shared_ptr<client_type> s);

   int on_join_group_ack(json j, std::shared_ptr<client_type> s);
   int on_ok_join_group_ack(json j, std::shared_ptr<client_type> s);

   int number_of_valid_group_msgs = 10;
   int number_of_group_msgs = 4;

   // Functions called when new message arrives. If any of these
   // functions return -1 it means the server returned something wrong
   // and we should make the test fail.

   int on_chat_message(json j, std::shared_ptr<client_type> s);
   int on_send_group_msg_ack(json j, std::shared_ptr<client_type> s);

   void send_group_msg(std::shared_ptr<client_type> s);
   void send_user_msg(std::shared_ptr<client_type> s);

   void send_msg(std::string msg, std::shared_ptr<client_type> s);

public:
   client_mgr(std::string tel_);
   user_bind bind;
   int on_read(json j, std::shared_ptr<client_type> s);
   int on_fail_read(boost::system::error_code ec);
   int on_write(std::shared_ptr<client_type> s);
   int on_handshake(std::shared_ptr<client_type> s);
   //void prompt_login();
   //void prompt_create_group();
   //void prompt_join_group();
   //void prompt_send_group_msg();
   //void prompt_send_user_msg();
   //void prompt_close();
};


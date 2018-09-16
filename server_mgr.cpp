#include "server_mgr.hpp"
#include "server_session.hpp"

index_type
server_mgr::on_read(json j, std::shared_ptr<server_session> s)
{
   //std::cout << j << std::endl;
   auto cmd = j["cmd"].get<std::string>();

   if (s->is_waiting_auth()) {
      if (cmd == "login")
         return on_login(std::move(j), s);

      if (cmd == "auth")
         return on_auth(std::move(j), s);

      std::cerr << "Server: Unknown command " << cmd << std::endl;
      return -1;
   }

   if (s->is_waiting_sms()) {
      if (cmd == "sms_confirmation")
         return on_sms_confirmation(std::move(j), s);

      std::cerr << "Server: Unknown command " << cmd << std::endl;
      return -1;
   }

   if (s->is_auth()) {
      if (cmd == "create_group")
         return on_create_group(std::move(j), s);

      if (cmd == "join_group")
         return on_join_group(std::move(j), s);

      if (cmd == "send_group_msg")
         return on_group_msg(std::move(j), s);

      if (cmd == "user_msg")
         return on_user_msg(std::move(j), s);

      std::cerr << "Server: Unknown command " << cmd << std::endl;
      return -1;
   }

   std::cerr << "Server: Unknown command " << cmd << std::endl;
   return -1;
}

index_type server_mgr::on_login(json j, std::shared_ptr<server_session> s)
{
   // When the user downloads the app and clicks something like login
   // the app will send us the user phone.
   auto const tel = j["tel"].get<std::string>();

   auto const new_user = id_to_idx_map.insert({tel, {}});
   if (!new_user.second) {
      // The user already exists in the system. This case can be
      // triggered by some conditions
      //
      // 1. The user lost his phone and the number was assigned to
      //    a new person.
      // 2. The user reinstalled the app.
      // 3. There is an error in the app and it is sending us more
      //    than one login.
      //
      // I think the best strategy here is to refuse the login. Review
      // this later.
      json resp;
      resp["cmd"] = "login_ack";
      resp["result"] = "fail";
      s->send_msg(resp.dump());
      return -1;
   }

   // The user does not exist in the system we allocate an entry in
   // the users array.
   new_user.first->second = users.allocate();
   auto const idx = new_user.first->second; // Alias.

   if (idx == -1) {
      // We run out of memory. We have to release the element we just
      // inserted in the map.
      id_to_idx_map.erase(new_user.first);
      json resp;
      resp["cmd"] = "login_ack";
      resp["result"] = "fail";
      s->send_msg(resp.dump());
      return -1;
   }

   // TODO: Use a random number generator with six digits.
   s->set_sms("8347");
   s->set_login_idx(idx);

   json resp;
   resp["cmd"] = "login_ack";
   resp["result"] = "ok";
   s->send_msg(resp.dump());
   return 1;
}

index_type server_mgr::on_auth(json j, std::shared_ptr<server_session> s)
{
   auto const from = j["from"].get<user_bind>();

   if (!users.is_valid_index(from.index)) {
      // Not even a valid user. Just drop the connection.
      return -1;
   }

   if (from.tel != users[from.index].get_id()) {
      // Incorrect id.
      json resp;
      resp["cmd"] = "auth_ack";
      resp["result"] = "fail";
      s->send_msg(resp.dump());
      return -1;
   }

   s->set_login_idx(from.index);
   s->promote();
   users[from.index].store_session(s);

   json resp;
   resp["cmd"] = "auth_ack";
   resp["result"] = "ok";
   s->send_msg(resp.dump());
   return 3;
}

void server_mgr::release_login(index_type idx)
{
   auto const id = users[idx].get_id();
   users.deallocate(idx);
   id_to_idx_map.erase(id);
}

index_type
server_mgr::on_sms_confirmation(json j, std::shared_ptr<server_session> s)
{
   auto const tel = j["tel"].get<std::string>();
   auto const sms = j["sms"].get<std::string>();

   if (sms != s->get_sms()) {
      // TODO: Resend an sms to the user (some more times). For now
      // we will simply drop the connection and release resources.
      // First we remove this user from the index map.
      id_to_idx_map.erase(tel);
      json resp;
      resp["cmd"] = "sms_confirmation_ack";
      resp["result"] = "fail";
      s->send_msg(resp.dump());
      // We do not release the login index here but let the session
      // destructor do it for us.
      return -1;
   }

   s->promote();
   auto const idx = s->get_user_idx();
   users[idx].store_session(s);
   users[idx].set_id(tel);

   json resp;
   resp["cmd"] = "sms_confirmation_ack";
   resp["result"] = "ok";
   resp["user_bind"] = user_bind {tel, host, idx};

   users[idx].send_msg(resp.dump());
   return 2;
}

index_type
server_mgr::on_create_group(json j, std::shared_ptr<server_session> s)
{
   auto from = j["from"].get<user_bind>();

   if (!users.is_valid_index(from.index)) {
      // This is not even an existing user. Perhaps the json command
      // was sent with the wrong information signaling a logic error
      // in the app. I do not think we have to report this problem.
      return -1;
   }

   auto const hash = j["hash"].get<std::string>();

   auto const new_group = groups.insert({hash, {}});
   if (!new_group.second) {
      // Group already exists.
      json resp;
      resp["cmd"] = "create_group_ack";
      resp["result"] = "fail";
      users[from.index].send_msg(resp.dump());
      //std::cout << "fail" << j << std::endl;
      return from.index;
   }

   //std::cout << "ok" << j << std::endl;
   json resp;
   resp["cmd"] = "create_group_ack";
   resp["result"] = "ok";

   users[from.index].send_msg(resp.dump());
   return 4;
}

index_type
server_mgr::on_join_group(json j, std::shared_ptr<server_session> s)
{
   auto from = j["from"].get<user_bind>();
   if (!users.is_valid_index(from.index)) {
      // TODO: This indicates a section was athetified but has somehow
      // the wrong user bind. There a logic error somewhere. It should
      // be logged.
      return -1;
   }

   auto const hash = j["hash"].get<std::string>();

   auto const g = groups.find(hash);
   if (g == std::end(groups)) {
      json resp;
      resp["cmd"] = "join_group_ack";
      resp["result"] = "fail";
      s->send_msg(resp.dump());
      return 5;
   }

   g->second.add_member(from.tel, s);

   json resp;
   resp["cmd"] = "join_group_ack";
   resp["result"] = "ok";

   s->send_msg(resp.dump());
   return from.index;
}

index_type
server_mgr::on_group_msg(json j, std::shared_ptr<server_session> s)
{
   // TODO: On all messages that have a "from" field we should check
   // if this field matches the one stored in the session. This can be
   // used to avoid some kind of hacking like sending a message with
   // an identity that is different from the one checked in the
   // aithentification.
   auto const from = j["from"].get<user_bind>();
   if (!users.is_valid_index(from.index)) {
      // TODO: This indicates a section was athetified but has somehow
      // the wrong user bind. There a logic error somewhere. It should
      // be logged.
      return -1;
   }

   auto const to = j["to"].get<std::string>();
   auto const g = groups.find(to);
   if (g == std::end(groups)) {
      // This is a non-existing group. Perhaps the json command was
      // sent with the wrong information signaling a logic error in
      // the app.

      json resp;
      resp["cmd"] = "send_group_msg_ack";
      resp["result"] = "fail";
      users[from.index].send_msg(resp.dump());
      return 6;
   }

   json resp;
   resp["cmd"] = "group_msg";
   resp["body"] = j["msg"].get<std::string>();

   // TODO: Change broadcast to return the number of users that the
   // message has reached.
   g->second.broadcast_msg(resp.dump());

   json ack;
   ack["cmd"] = "send_group_msg_ack";
   ack["result"] = "ok";
   users[from.index].send_msg(ack.dump());

   return from.index;
}

index_type
server_mgr::on_user_msg(json j, std::shared_ptr<server_session> s)
{
   auto from = j["from"].get<user_bind>();
   if (!users.is_valid_index(from.index)) {
      // This is a non-existing user. Perhaps the json command was
      // sent with the wrong information signaling a logic error in
      // the app.
      // TODO: Decide what to do here.
      return -1;
   }

   auto to = j["to"].get<int>();

   if (!users.is_valid_index(to)) {
      // This is a non-existing user. Perhaps the json command was
      // sent with the wrong information signaling a logic error in
      // the app.
      // TODO: Decide what to do here.
      return from.index;
   }

   users[from.index].store_session(s);

   json resp;
   resp["cmd"] = "message";
   resp["message"] = j["msg"].get<std::string>();

   users[to].send_msg(resp.dump());
   return from.index;
}

void server_mgr::on_write(index_type user_idx)
{
   if (!users.is_valid_index(user_idx)) {
      // For example when we return -2 to the session for sms
      // confirmation. We can safely ignore this case.
      return;
   }

   users[user_idx].on_write();
}


#include "server_mgr.hpp"
#include "server_session.hpp"

ev_res
server_mgr::on_read(json j, std::shared_ptr<server_session> s)
{
   //std::cout << j << std::endl;
   auto const cmd = j["cmd"].get<std::string>();

   if (s->is_waiting_auth()) {
      if (cmd == "login")
         return on_login(std::move(j), s);

      if (cmd == "auth")
         return on_auth(std::move(j), s);

      std::cerr << "Server: Unknown command " << cmd << std::endl;
      return ev_res::UNKNOWN;
   }

   if (s->is_waiting_sms()) {
      if (cmd == "sms_confirmation")
         return on_sms_confirmation(std::move(j), s);

      std::cerr << "Server: Unknown command " << cmd << std::endl;
      return ev_res::UNKNOWN;
   }

   if (s->is_auth()) {
      if (cmd == "create_group")
         return on_create_group(std::move(j), s);

      if (cmd == "join_group")
         return on_join_group(std::move(j), s);

      if (cmd == "group_msg")
         return on_group_msg(std::move(j), s);

      if (cmd == "user_msg")
         return on_user_msg(std::move(j), s);

      std::cerr << "Server: Unknown command " << cmd << std::endl;
      return ev_res::UNKNOWN;
   }

   std::cerr << "Server: Unknown command " << cmd << std::endl;
   return ev_res::UNKNOWN;
}

ev_res server_mgr::on_login(json j, std::shared_ptr<server_session> s)
{
   auto const tel = j["tel"].get<std::string>();

   if (id_to_idx_map.find(tel) != std::end(id_to_idx_map)) {
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
      return ev_res::LOGIN_FAIL;
   }

   // The user does not exist in the system so we allocate an entry in
   // the users array and wait for sms confirmation. If confirmation
   // does not happen (we timeout first) then this index should be
   // released. This happens on session destruction.
   auto const idx = user_idx_mgr.allocate();

   if (idx == -1) {
      json resp;
      resp["cmd"] = "login_ack";
      resp["result"] = "fail";
      s->send_msg(resp.dump());
      return ev_res::LOGIN_FAIL;
   }

   // WARNING: Ensure the release of idx is exception safe.
   s->set_login_idx(idx);

   // TODO: Use a random number generator with six digits.
   s->set_sms("8347");

   json resp;
   resp["cmd"] = "login_ack";
   resp["result"] = "ok";
   s->send_msg(resp.dump());
   return ev_res::LOGIN_OK;
}

ev_res server_mgr::on_auth(json j, std::shared_ptr<server_session> s)
{
   auto const from = j["from"].get<user_bind>();

   if (from.tel != users.at(from.index).get_id()) {
      // Incorrect id.
      json resp;
      resp["cmd"] = "auth_ack";
      resp["result"] = "fail";
      s->send_msg(resp.dump());
      return ev_res::AUTH_FAIL;
   }

   s->set_login_idx(from.index);
   s->promote();
   users[from.index].store_session(s);

   json resp;
   resp["cmd"] = "auth_ack";
   resp["result"] = "ok";
   s->send_msg(resp.dump());
   return ev_res::AUTH_OK;
}

void server_mgr::release_login(index_type idx)
{
   // Releases back the index allocated on the login. This is caused
   // by a timeout of the sms confirmation.
   user_idx_mgr.deallocate(idx);
}

ev_res
server_mgr::on_sms_confirmation(json j, std::shared_ptr<server_session> s)
{
   auto const tel = j["tel"].get<std::string>();
   auto const sms = j["sms"].get<std::string>();

   if (sms != s->get_sms()) {
      // TODO: Resend an sms to the user (some more times). For now
      // we will simply drop the connection and release resources.
      json resp;
      resp["cmd"] = "sms_confirmation_ack";
      resp["result"] = "fail";
      s->send_msg(resp.dump());
      // We do not release the login index here but let the session
      // destructor do it for us.
      return ev_res::SMS_CONFIRMATION_FAIL;
   }

   // Before we call s->get_user_idx() we have to promete the session.
   s->promote();

   // Inserts the user in the system.
   auto const idx = s->get_user_idx();
   assert(idx != -1);
   auto const new_user = id_to_idx_map.insert({tel, idx});

   // This would be odd. The entry already exists on the index map
   // which means we did something wrong in the login command.
   assert(new_user.second);

   users[idx].store_session(s);
   users[idx].set_id(tel);

   json resp;
   resp["cmd"] = "sms_confirmation_ack";
   resp["result"] = "ok";
   resp["user_bind"] = user_bind {tel, host, idx};

   users[idx].send_msg(resp.dump());
   return ev_res::SMS_CONFIRMATION_OK;
}

ev_res
server_mgr::on_create_group(json j, std::shared_ptr<server_session> s)
{
   auto const from = j["from"].get<user_bind>();
   auto const hash = j["hash"].get<std::string>();

   auto const new_group = groups.insert({hash, {}});
   if (!new_group.second) {
      // Group already exists.
      json resp;
      resp["cmd"] = "create_group_ack";
      resp["result"] = "fail";
      users.at(from.index).send_msg(resp.dump());
      //std::cout << "fail" << j << std::endl;
      return ev_res::CREATE_GROUP_FAIL;
   }

   //std::cout << "ok" << j << std::endl;
   json resp;
   resp["cmd"] = "create_group_ack";
   resp["result"] = "ok";

   users.at(from.index).send_msg(resp.dump());
   return ev_res::CREATE_GROUP_OK;
}

ev_res
server_mgr::on_join_group(json j, std::shared_ptr<server_session> s)
{
   auto const from = j["from"].get<user_bind>();
   auto const hash = j["hash"].get<std::string>();

   auto const g = groups.find(hash);
   if (g == std::end(groups)) {
      json resp;
      resp["cmd"] = "join_group_ack";
      resp["result"] = "fail";
      s->send_msg(resp.dump());
      return ev_res::JOIN_GROUP_FAIL;
   }

   g->second.add_member(from.tel, s);

   json resp;
   resp["cmd"] = "join_group_ack";
   resp["result"] = "ok";

   s->send_msg(resp.dump());
   return ev_res::JOIN_GROUP_OK;
}

ev_res
server_mgr::on_group_msg(json j, std::shared_ptr<server_session> s)
{
   auto const from = j["from"].get<user_bind>();
   auto const to = j["to"].get<std::string>();

   auto const g = groups.find(to);
   if (g == std::end(groups)) {
      // This is a non-existing group. Perhaps the json command was
      // sent with the wrong information signaling a logic error in
      // the app.

      json resp;
      resp["cmd"] = "group_msg_ack";
      resp["result"] = "fail";
      users.at(from.index).send_msg(resp.dump());
      return ev_res::GROUP_MSG_FAIL;
   }

   // TODO: Change broadcast to return the number of users that the
   // message has reached.
   g->second.broadcast_msg(j.dump());

   json ack;
   ack["cmd"] = "group_msg_ack";
   ack["result"] = "ok";
   users.at(from.index).send_msg(ack.dump());

   return ev_res::GROUP_MSG_OK;
}

ev_res
server_mgr::on_user_msg(json j, std::shared_ptr<server_session> s)
{
   auto const from = j["from"].get<user_bind>();

   auto const to = j["to"].get<int>();

   users.at(from.index).store_session(s);

   json resp;
   resp["cmd"] = "message";
   resp["message"] = j["msg"].get<std::string>();

   users.at(to).send_msg(resp.dump());
   return ev_res::USER_MSG_OK;
}

void server_mgr::on_write(index_type user_idx)
{
   users[user_idx].on_write();
}


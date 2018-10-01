#include "server_mgr.hpp"
#include "server_session.hpp"

ev_res
server_mgr::on_read(std::string msg, std::shared_ptr<server_session> s)
{
   auto const j = json::parse(msg);
   //std::cout << j << std::endl;
   auto const cmd = j.at("cmd").get<std::string>();

   if (cmd == "create_group")
      return on_create_group(std::move(j), s);

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

      if (cmd == "join_group")
         return on_join_group(std::move(j), s);

      if (cmd == "group_msg")
         return on_group_msg(std::move(msg), std::move(j), s);

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
   auto const tel = j.at("tel").get<std::string>();

   // TODO: Replace this with a query to the database.
   if (sessions.find(tel) != std::end(sessions)) {
      // The user already exists in the system.
      json resp;
      resp["cmd"] = "login_ack";
      resp["result"] = "fail";
      s->send_msg(resp.dump());
      return ev_res::LOGIN_FAIL;
   }

   s->set_user_id(tel);

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
   auto const from = j.at("from").get<std::string>();

   auto const new_user = sessions.insert({from, s});
   if (!new_user.second) {
      // The user is already logged into the system. We do not allow
      // this yet.
      json resp;
      resp["cmd"] = "auth_ack";
      resp["result"] = "fail";
      s->send_msg(resp.dump());
      return ev_res::AUTH_FAIL;
   }

   // TODO: Query the database to validate the session.
   //if (from != s->get_user_id()) {
   //   // Incorrect id.
   //   json resp;
   //   resp["cmd"] = "auth_ack";
   //   resp["result"] = "fail";
   //   s->send_msg(resp.dump());
   //   return ev_res::AUTH_FAIL;
   //}

   s->set_user_id(from);
   s->promote();
   //new_user.first.second->set_session(s);

   json resp;
   resp["cmd"] = "auth_ack";
   resp["result"] = "ok";
   s->send_msg(resp.dump());
   return ev_res::AUTH_OK;
}

ev_res
server_mgr::on_sms_confirmation(json j, std::shared_ptr<server_session> s)
{
   auto const tel = j.at("tel").get<std::string>();
   auto const sms = j.at("sms").get<std::string>();

   if (sms != s->get_sms()) {
      json resp;
      resp["cmd"] = "sms_confirmation_ack";
      resp["result"] = "fail";
      s->send_msg(resp.dump());
      return ev_res::SMS_CONFIRMATION_FAIL;
   }

   s->promote();

   // Inserts the user in the system.
   auto const id = s->get_user_id();
   assert(!std::empty(id));
   auto const new_user = sessions.insert({tel, s});

   // This would be odd. The entry already exists on the index map
   // which means we did something wrong in the login command.
   assert(new_user.second);

   json resp;
   resp["cmd"] = "sms_confirmation_ack";
   resp["result"] = "ok";
   s->send_msg(resp.dump());
   return ev_res::SMS_CONFIRMATION_OK;
}

ev_res
server_mgr::on_create_group(json j, std::shared_ptr<server_session> s)
{
   auto const hash = j.at("hash").get<std::string>();

   auto const new_group = groups.insert({hash, {}});
   if (!new_group.second) {
      // Group already exists.
      json resp;
      resp["cmd"] = "create_group_ack";
      resp["result"] = "fail";
      s->send_msg(resp.dump());
      //std::cout << "fail" << j << std::endl;
      return ev_res::CREATE_GROUP_FAIL;
   }

   //std::cout << "ok" << j << std::endl;
   json resp;
   resp["cmd"] = "create_group_ack";
   resp["result"] = "ok";

   s->send_msg(resp.dump());
   return ev_res::CREATE_GROUP_OK;
}

ev_res
server_mgr::on_join_group(json j, std::shared_ptr<server_session> s)
{
   auto const hash = j.at("hash").get<std::string>();

   auto const g = groups.find(hash);
   if (g == std::end(groups)) {
      json resp;
      resp["cmd"] = "join_group_ack";
      resp["result"] = "fail";
      s->send_msg(resp.dump());
      return ev_res::JOIN_GROUP_FAIL;
   }

   auto const from = s->get_user_id();
   g->second.add_member(from, s);

   json resp;
   resp["cmd"] = "join_group_ack";
   resp["result"] = "ok";

   s->send_msg(resp.dump());
   return ev_res::JOIN_GROUP_OK;
}

ev_res
server_mgr::on_group_msg( std::string msg
                        , json j
                        , std::shared_ptr<server_session> s)
{
   auto const to = j.at("to").get<std::string>();

   auto const g = groups.find(to);
   if (g == std::end(groups)) {
      // This is a non-existing group. Perhaps the json command was
      // sent with the wrong information signaling a logic error in
      // the app.

      json resp;
      resp["cmd"] = "group_msg_ack";
      resp["result"] = "fail";
      s->send_msg(resp.dump());
      return ev_res::GROUP_MSG_FAIL;
   }

   // TODO: Change broadcast to return the number of users that the
   // message has reached.
   g->second.broadcast_msg(std::move(msg));

   json ack;
   ack["cmd"] = "group_msg_ack";
   ack["result"] = "ok";
   s->send_msg(ack.dump());

   return ev_res::GROUP_MSG_OK;
}

void server_mgr::release_user(std::string id)
{
   sessions.erase(id);
}

ev_res
server_mgr::on_user_msg(json j, std::shared_ptr<server_session> s)
{
   //auto const from = j["from"].get<user_bind>();
   //auto const to = j["to"].get<user_bind>();

   //users.at(to.index).send_msg(j.dump());
   return ev_res::USER_MSG_OK;
}

void server_mgr::shutdown()
{
   std::cout << "Shutting down user sessions ..." << std::endl;

   //for (auto& o : users)
   //   o.shutdown();
}


#include "server_data.hpp"
#include "server_session.hpp"

index_type
server_data::on_read(json j, std::shared_ptr<server_session> session)
{
   //std::cout << j << std::endl;
   auto cmd = j["cmd"].get<std::string>();
   if (cmd == "login") {
      return on_login(std::move(j), session);
   } else if (cmd == "create_group") {
      return on_create_group(std::move(j), session);
   } else if (cmd == "join_group") {
      return on_join_group(std::move(j), session);
   } else if (cmd == "send_group_msg") {
      return on_group_msg(std::move(j), session);
   } else if (cmd == "send_user_msg") {
      return on_user_msg(std::move(j), session);
   } else {
      std::cerr << "Server: Unknown command " << cmd << std::endl;
      return -1;
   }
}

index_type server_data::on_login(json j, std::shared_ptr<server_session> s)
{
   auto tel = j["tel"].get<std::string>();
   auto name = j["name"].get<std::string>();

   //std::cout << "New login from " << name << " " << tel
   //          << std::endl;

   index_type new_user_idx = -1;
   auto new_user = id_to_idx_map.insert({tel, {}});
   if (!new_user.second) {
      // The user already exists in the system. This case can be
      // triggered by some conditions
      // 1. The user lost his phone and the number was assigned to
      //    a new person.
      // 2. He reinstalled the app.
      //
      // REVIEW: I still do not know how to handle this sitiations
      // properly so I am not going to do anything for now. We
      // could for example reset his data and send him an SMS for
      // confimation. 
      new_user_idx = new_user.first->second;
   } else {
      // The user did not exist in the system. We have to allocate
      // space for him.
      new_user_idx = users.allocate();
   }

   if (new_user_idx == -1) {
      // TODO: Send the user a message reporting some error.
      return -1; // We run out of memory.
   }

   // TODO: The following piece of code is very critical, if an
   // exception is thrown we will not release new_user_idx back to the
   // users array causing a leak. We have to eleaborate some way to
   // use RAII.

   users[new_user_idx].store_session(s);
   users[new_user_idx].set_id(std::move(tel));

   json resp;
   resp["cmd"] = "login_ack";
   resp["result"] = "ok";
   resp["user_idx"] = new_user_idx;

   users[new_user_idx].send_msg(resp.dump());
   return new_user_idx;
}

index_type
server_data::on_group_msg(json j, std::shared_ptr<server_session> s)
{
   auto from = j["from"].get<int>();
   if (!users.is_valid_index(from)) {
      // This is a non-existing user. Perhaps the json command was
      // sent with the wrong information signaling a logic error in
      // the app.
      // TODO: Decide what to do here.
      return -1;
   }

   users[from].store_session(s);

   auto to = j["to"].get<int>();

   if (!groups.is_valid_index(to)) {
      // This is a non-existing group. Perhaps the json command was
      // sent with the wrong information signaling a logic error in
      // the app.

      json resp;
      resp["cmd"] = "send_group_msg_ack";
      resp["result"] = "fail";
      resp["reason"] = "Non existing group";
      users[from].send_msg(resp.dump());
      return from;
   }

   if (!groups[to].is_active()) {
      // TODO: Report back to the user that this groups does no exist
      // anymore.
      return from;
   }

   json resp;
   resp["cmd"] = "message";
   resp["message"] = j["msg"].get<std::string>();

   groups[to].broadcast_msg(resp.dump(), users);

   // We also have to acknowledge the broadcast of the message.
   // Perhaps this is only for debugging purposes as the user can
   // inferr himself if the message was sent by looking if the message
   // arrived in the corresponding group.
   json ack;
   ack["cmd"] = "send_group_msg_ack";
   ack["result"] = "ok";
   users[from].send_msg(ack.dump());

   return from;
}

index_type
server_data::on_user_msg(json j, std::shared_ptr<server_session> s)
{
   auto from = j["from"].get<int>();
   if (!users.is_valid_index(from)) {
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
      return from;
   }

   users[from].store_session(s);

   json resp;
   resp["cmd"] = "message";
   resp["message"] = j["msg"].get<std::string>();

   users[to].send_msg(resp.dump());
   return from;
}

index_type server_data::on_create_group(json j, std::shared_ptr<server_session> s)
{
   auto from = j["from"].get<int>();

   // Before allocating a new group it is a good idea to check if
   // the owner passed is at least in a valid range.
   if (!users.is_valid_index(from)) {
      // This is a non-existing user. Perhaps the json command was
      // sent with the wrong information signaling a logic error in
      // the app.
      json resp;
      resp["cmd"] = "create_group_ack";
      resp["result"] = "fail";
      //s->write(resp.dump());

      // TODO: Review what should be returned here.
      return -1;
   }

   users[from].store_session(s);

   auto idx = groups.allocate();
   if (idx == -1) {
      // We run out of memory in this server and this group must be
      // created else where. This situation must be thought carefully
      // I still donot know if this can happen. If so many groups will
      // ever be created.

      json resp;
      resp["cmd"] = "create_group_ack";
      resp["result"] = "fail";
      resp["reason"] = "Out of memory";
      users[from].send_msg(resp.dump());
      return from;
   }

   auto info = j["info"].get<group_info>();
   groups[idx].set_owner(from);
   groups[idx].set_info(std::move(info));
   groups[idx].add_member(from);

   users[from].add_group(idx);

   json resp;
   resp["cmd"] = "create_group_ack";
   resp["result"] = "ok";
   resp["group_id"] = idx;

   users[from].send_msg(resp.dump());
   return from;
}

group server_data::remove_group(index_type idx)
{
   if (!groups.is_valid_index(idx))
      return group {}; // Out of range? Logic error.

   // To remove a group we have to inform its members the group has
   // been removed, therefore we will make a copy before removing
   // and return it.
   const auto removed_group = std::move(groups[idx]);

   // remove this line after implementing the swap idiom on the
   // group class.
   groups[idx].reset();

   groups.deallocate(idx);

   // Now we have to remove this group from the owners list.
   const auto owner = removed_group.get_owner();
   users[owner].remove_group(idx);
   return removed_group; // Let RVO optimize this.
}

bool server_data::change_group_ownership( index_type from, index_type to
                                        , index_type gid)
{
   if (!groups.is_valid_index(gid))
      return false;

   if (!groups[gid].is_owned_by(from))
      return false; // Sorry, you are not allowed.

   if (!users.is_valid_index(from))
      return false;

   if (!users.is_valid_index(to))
      return false;

   // The new owner exists.
   groups[gid].set_owner(to);
   users[to].add_group(gid);
   users[from].remove_group(gid);
   return true;
}

index_type
server_data::on_join_group(json j, std::shared_ptr<server_session> s)
{
   auto from = j["from"].get<int>();
   if (!users.is_valid_index(from)) {
      // TODO: Clarify how this could happen.
      return -1;
   }

   users[from].store_session(s);

   auto gid = j["group_id"].get<int>();

   if (!groups.is_valid_index(gid)) {
      // TODO: Clarify how this could happen.
      json resp;
      resp["cmd"] = "join_group_ack";
      resp["result"] = "fail";
      resp["reason"] = "Invalid group id.";
      users[from].send_msg(resp.dump());
      return from;
   }

   if (!groups[gid].is_active()) {
      // TODO: Clarify how this could happen.
      json resp;
      resp["cmd"] = "join_group_ack";
      resp["result"] = "fail";
      resp["reason"] = "Group not active.";
      users[from].send_msg(resp.dump());
      return from;
   }

   groups[gid].add_member(from);

   json resp;
   resp["cmd"] = "join_group_ack";
   resp["result"] = "ok";
   resp["info"] = groups[gid].get_info(),

   users[from].send_msg(resp.dump());
   return from;
}

void server_data::on_write(index_type user_idx)
{
   if (!users.is_valid_index(user_idx)) {
      // TODO: Clarify how this could happen.
      return;
   }

   users[user_idx].on_write();
}


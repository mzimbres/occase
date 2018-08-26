#include "server_mgr.hpp"
#include "server_session.hpp"

index_type
server_mgr::on_read(json j, std::shared_ptr<server_session> s)
{
   //std::cout << j << std::endl;
   auto cmd = j["cmd"].get<std::string>();
   if (cmd == "login") {
      return on_login(std::move(j), s);
   } else if (cmd == "sms_confirmation") {
      return on_sms_confirmation(std::move(j), s);
   } else if (cmd == "create_group") {
      return on_create_group(std::move(j), s);
   } else if (cmd == "join_group") {
      return on_join_group(std::move(j), s);
   } else if (cmd == "send_group_msg") {
      return on_group_msg(std::move(j), s);
   } else if (cmd == "send_user_msg") {
      return on_user_msg(std::move(j), s);
   } else {
      std::cerr << "Server: Unknown command " << cmd << std::endl;
      return -1;
   }
}

index_type server_mgr::on_login(json j, std::shared_ptr<server_session> s)
{
   auto tel = j["tel"].get<std::string>();

   index_type new_user_idx = -1;
   auto new_user = id_to_idx_map.insert({tel, {}});
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
      // REVIEW: I think the best strategy here is to reset the user
      // data to avoid leaking it to whatever person the number
      // happened to be assigned to. If the user uninstalled the app
      // himself, them he may be ok with losing his data. 
      //
      // This is where we will send the user an SMS for
      // confirmation he is the owner of this number.
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

   // TODO: Set a timeout on the sms code.
   users[new_user_idx].store_session(s);
   users[new_user_idx].set_sms("8347");

   json resp;
   resp["cmd"] = "login_ack";
   resp["result"] = "ok";

   // TODO: Study what how should we behave if the client closes the
   // connection here.
   users[new_user_idx].send_msg(resp.dump());
   return new_user_idx;
}

index_type
server_mgr::on_sms_confirmation(json j, std::shared_ptr<server_session> s)
{
   auto tel = j["tel"].get<std::string>();

   auto new_user = id_to_idx_map.insert({tel, {}});
   if (new_user.second) {
      // This situation should not happen on a normal behaviour. If
      // the user is confirming the sms he must already sent us login.
      // If not, them it will be considered missuse and drop the
      // connection.
      std::cout << "aaaaaa" << std::endl;
      return -1;
   }

   auto idx = new_user.first->second;

   // TODO: The following piece of code is very critical, if an
   // exception is thrown we will not release idx back to the users
   // array causing a leak. We have to eleaborate some way to use
   // RAII.
   auto sms = j["sms"].get<std::string>();

   if (sms != users[idx].get_sms()) {
      // TODO: Resend and sms to the user (some more times). For now
      // we will simply drop the connection and release resources.
      //json resp;
      //resp["cmd"] = "sms_confirmation_ack";
      //resp["result"] = "fail";
      users.deallocate(idx);
      id_to_idx_map.erase(new_user.first);
      std::cout << "bbbbbb" << std::endl;
      return -1;
   }

   users[idx].reset();
   users[idx].set_id(tel);
   users[idx].store_session(s);

   json resp;
   resp["cmd"] = "sms_confirmation_ack";
   resp["result"] = "ok";
   resp["user_bind"] = user_bind {tel, host, idx};

   users[idx].send_msg(resp.dump());
   return idx;
}

index_type
server_mgr::on_group_msg(json j, std::shared_ptr<server_session> s)
{
   auto from = j["from"].get<user_bind>();
   if (!users.is_valid_index(from.index)) {
      // This is a non-existing user. Perhaps the json command was
      // sent with the wrong information signaling a logic error in
      // the app.
      // TODO: Decide what to do here.
      return -1;
   }

   users[from.index].store_session(s);

   auto to = j["to"].get<int>();

   if (!groups.is_valid_index(to)) {
      // This is a non-existing group. Perhaps the json command was
      // sent with the wrong information signaling a logic error in
      // the app.

      json resp;
      resp["cmd"] = "send_group_msg_ack";
      resp["result"] = "fail";
      resp["reason"] = "Non existing group";
      users[from.index].send_msg(resp.dump());
      return from.index;
   }

   if (!groups[to].is_active()) {
      // TODO: Report back to the user that this groups does no exist
      // anymore.
      return from.index;
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

index_type
server_mgr::on_create_group(json j, std::shared_ptr<server_session> s)
{
   auto from = j["from"].get<user_bind>();

   // Before allocating a new group it is a good idea to check if
   // the owner passed is at least in a valid range.
   auto b = users.is_valid_index(from.index);
   if (!b) {
      // This is not even an existing user. Perhaps the json command
      // was sent with the wrong information signaling a logic error
      // in the app. I do not think we have to report this problem.
      return -1;
   }

   if (from.tel != users[from.index].get_id()) {
      // The user telephone number and its index do not match. Causes
      // of this is unclear, maybe someone scanning all possible
      // indexes to try hack the server?
      return -1;
   }

   users[from.index].store_session(s);

   auto info = j["info"].get<group_info>();

   auto idx = groups.allocate();
   if (idx == -1) {
      // In my current design the total number of groups we not grow
      // dynamically but be set on startup. I may have to change this
      // later or start the server with some room for dynamic creation
      // of groups. However we can always restart the server, though
      // this is very inconvenient. It is important to report this to
      // the user, that in this case is probably the admin.
      json resp;
      resp["cmd"] = "create_group_ack";
      resp["result"] = "fail";
      users[from.index].send_msg(resp.dump());
      return from.index;
   }

   groups[idx].set_owner(from.index);
   groups[idx].set_info(std::move(info));
   groups[idx].add_member(from.index);

   users[from.index].add_group(idx);

   json resp;
   resp["cmd"] = "create_group_ack";
   resp["result"] = "ok";
   resp["group_bind"] = group_bind {host, idx};

   users[from.index].send_msg(resp.dump());
   return from.index;
}

group server_mgr::remove_group(index_type idx)
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

bool server_mgr::change_group_ownership( index_type from, index_type to
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
server_mgr::on_join_group(json j, std::shared_ptr<server_session> s)
{
   auto from = j["from"].get<user_bind>();
   if (!users.is_valid_index(from.index)) {
      // TODO: Clarify how this could happen.
      return -1;
   }

   if (from.tel != users[from.index].get_id()) {
      return -1;
   }

   users[from.index].store_session(s);

   auto gbind = j["group_bind"].get<group_bind>();

   if (!groups.is_valid_index(gbind.index)) {
      // TODO: Clarify how this could happen.
      //json resp;
      //resp["cmd"] = "join_group_ack";
      //resp["result"] = "fail";
      //users[from.index].send_msg(resp.dump());
      return -1;
   }

   if (!groups[gbind.index].is_active()) {
      // TODO: Clarify how this could happen.
      //json resp;
      //resp["cmd"] = "join_group_ack";
      //resp["result"] = "fail";
      //users[from.index].send_msg(resp.dump());
      return -1;
   }

   groups[gbind.index].add_member(from.index);

   json resp;
   resp["cmd"] = "join_group_ack";
   resp["result"] = "ok";
   resp["info"] = groups[gbind.index].get_info(),

   users[from.index].send_msg(resp.dump());
   return from.index;
}

void server_mgr::on_write(index_type user_idx)
{
   if (!users.is_valid_index(user_idx)) {
      // TODO: Clarify how this could happen.
      return;
   }

   users[user_idx].on_write();
}


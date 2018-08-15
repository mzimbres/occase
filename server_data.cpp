#include "server_data.hpp"
#include "server_session.hpp"

void server_data::on_login(json j, std::shared_ptr<server_session> s)
{
   auto tel = j["tel"].get<std::string>();

   auto name = j["name"].get<std::string>();
   std::cout << "New login from " << name << " " << tel
             << std::endl;

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
      // could for example reset his data.
      new_user_idx = new_user.first->second;
   } else {
      // The user did not exist in the system. We have to allocate
      // space for him.
      new_user_idx = users.allocate();
   }

   // TODO: The following piece of code is very critical, if an
   // exception is thrown we will no release new_user_idx back to the
   // usrs array causing a leak. We have to eleaborate some way to use
   // RAII.
   users[new_user_idx].store_session(s);

   json resp;
   resp["cmd"] = "login_ack";
   resp["result"] = "ok";
   resp["user_idx"] = new_user_idx;

   users[new_user_idx].send_msg(resp.dump());
}

int server_data::send_group_msg(std::string const& msg, index_type to) const
{
   if (!groups.is_valid_index(to)) {
      // This is a non-existing user. Perhaps the json command was
      // sent with the wrong information signaling a logic error in
      // the app.
      return -1;
   }

   // Now we broadcast.
   groups[to].broadcast_msg(std::move(msg), users);
   return 1;
}

index_type server_data::create_group(index_type owner, group_info info)
{
   // Before allocating a new group it is a good idea to check if
   // the owner passed is at least in a valid range.
   if (!users.is_valid_index(owner)) {
      // This is a non-existing user. Perhaps the json command was
      // sent with the wrong information signaling a logic error in
      // the app.
      return static_cast<index_type>(-1);
   }

   // We can proceed and allocate the group.
   auto idx = groups.allocate();
   groups[idx].set_owner(owner);
   groups[idx].set_info(std::move(info));

   // Updates the user with his new group.
   users[owner].add_group(idx);

   return idx;
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

void
server_data::on_join_group(json j, std::shared_ptr<server_session> session)
{
   auto from = j["from"].get<int>();
   auto gid = j["group_idx"].get<int>();

   const auto b1 = groups.is_valid_index(gid);
   const auto b2 = users.is_valid_index(from);

   if (!b1 || !b2) {
      json resp;
      resp["cmd"] = "join_group_ack";
      resp["result"] = "fail";
      session->write(resp.dump());
      return;
   }

   // For safety we save the connection in case it expired.
   users[from].store_session(session);
   groups[gid].add_member(from);

   json resp;
   resp["cmd"] = "join_group_ack";
   resp["result"] = "ok";
   resp["info"] = groups[gid].get_info(),

   users[from].send_msg(resp.dump());
}


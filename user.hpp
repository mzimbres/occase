#pragma once

#include <set>
#include <queue>
#include <vector>
#include <memory>

#include "config.hpp"

class server_session;

// Remarks: This user struct will not store the groups it belongs to.
// This information can be obtained from the groups array in an
// indirect manner i.e. by traversing it and quering each group
// whether this user is a member. This is a very expensive operation
// and I am usure we need it. However, the app will have to store the
// groups it belongs to so that it can send messages to to group.
class user {
private:
   // At the moment I think this will be the user telephone.
   id_type id;

   std::weak_ptr<server_session> session;
   std::queue<std::string> msg_queue;

public:
   user(std::string id_ = {}) : id(std::move(id_)) {}
   ~user() = default;

   void add_friend(index_type uid);
   void remove_friend(index_type uid);

   // TODO: What to do? Should return true only if user still has an
   // account.
   auto is_active() const {return std::empty(id);}
   void add_group(index_type gid);
   void store_session(std::shared_ptr<server_session> s);
   void on_group_msg(std::string msg);
   void send_msg(std::string msg);
   void on_write();
   void reset();
   auto const& get_id() const noexcept {return id;}
   void set_id(std::string id_) {id = std::move(id_);}
};


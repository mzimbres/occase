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

   // The operations we are suposed to perform on a user's friends
   // are
   //
   // 1. Insert: On registration and as user adds or remove friends.
   // 2. Remove: On registration and as user adds or remove friends.
   // 3. Search. I do not think we will perform read-only searches.
   //
   // Revove should happens with even less frequency that insertion,
   // what makes this operation non-critical. The number of friends is
   // expected to be no more that 1000.  If we begin to perform
   // searche often, we may have to change this to a data structure
   // with faster lookups.
   std::set<index_type> friends;

   // The user is expected to create groups, of which he becomes the
   // owner. The total number of groups a user creates won't be high
   // on average, lets us say less than 20. The operations we are
   // expected to perform on the groups a user owns are
   //
   // 1. Insert rarely and always in the back O(1).
   // 2. Remove rarely O(n).
   // 3. Search ??? O(n) (No need now, clarify this).
   // 
   // A vector seems the most suitable for these requirements.
   std::vector<index_type> own_groups;

   // User websocket session.
   std::weak_ptr<server_session> session;

   std::queue<std::string> msg_queue;

public:
   user() = default;
   ~user() = default;

   void add_friend(index_type uid);
   void remove_friend(index_type uid);

   // TODO: What to do? Should return true only if user still has an
   // account.
   auto is_active() const {return std::empty(id);}
   // Removes group owned by this user from his list of groups.
   void remove_group(index_type group);
   void add_group(index_type gid);
   void store_session(std::shared_ptr<server_session> s);
   void send_msg(std::string msg);
   void on_write();
   void reset();
   void set_id(id_type id_) {id = std::move(id_);}
};


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
   id_type id; // User email or telephone number.

public:
   user(std::string id_ = {}) : id(std::move(id_)) {}
   ~user() = default;

   // TODO: What to do? Should return true only if user still has an
   // account.
   void reset() { id = {}; }
   auto const& get_id() const noexcept {return id;}
   void set_id(std::string id_) {id = std::move(id_);}
};


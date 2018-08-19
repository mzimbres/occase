#include "group.hpp"

void group::broadcast_msg( std::string msg
                         , grow_only_vector<user>& users) const
{
   for (auto const& user : members) {
      users[user.first].send_msg(msg);
   }
}


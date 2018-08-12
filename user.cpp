#include "user.hpp"

#include <algorithm>

#include "server_session.hpp"

void user::add_friend(index_type uid)
{
   friends.insert(uid);
}

void user::remove_friend(index_type uid)
{
   friends.erase(uid);
}

void user::remove_group(index_type group)
{
   auto group_match =
      std::remove( std::begin(own_groups), std::end(own_groups)
                 , group);

   own_groups.erase(group_match, std::end(own_groups));
}

void user::add_group(index_type gid)
{
   own_groups.push_back(gid);
}

void user::send_msg(std::string const& msg) const
{
   if (auto s = session.lock()) {
      s->write(std::move(msg));
   } else {
      std::cerr << "Session is expired." << std::endl;
   }
}


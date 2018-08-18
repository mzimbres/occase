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

void user::store_session(std::shared_ptr<server_session> s)
{
   // TODO: Makes sure that our session object stops existing when the
   // client cannot be reached anymore. It may be safer to simply set
   // the connection every time.
   if (session.use_count() == 0)
      session = s;
}

void user::send_msg(std::string msg)
{
   const auto is_empty = queue.empty();
   queue.push(std::move(msg));

   if (is_empty) {
      if (auto s = session.lock()) {
         s->write(queue.front());
         return;
      }

      // TODO: Decide what to do here.
      std::cerr << "Session is expired." << std::endl;
   }
}

void user::on_write()
{
   queue.pop();

   if (queue.empty())
      return; // No more message to send to the client.

   if (auto s = session.lock()) {
      s->write(queue.front());
      return;
   }

   // TODO: Decide what to do here.
   std::cerr << "Session is expired." << std::endl;
}


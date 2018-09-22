#include "user.hpp"

#include <algorithm>

#include "server_session.hpp"

void user::store_session(std::shared_ptr<server_session> s)
{
   session = s; // Creates a weak reference to the session.

   // Flush the user all messages he received while he was offline.
   while (!msg_queue.empty()) {
      s->send_msg(msg_queue.front());
      msg_queue.pop();
   }
}

void user::send_msg(std::string msg)
{
   if (auto s = session.lock()) {
      // The user is online. We can just forward his message.
      s->send_msg(std::move(msg));
      return;
   }

   // The user is offline. We will queue all his messages and send him
   // one he gets online again.
   msg_queue.push(std::move(msg));

   // Discard messages if the queue grew too big.
   if (std::size(msg_queue) > user_msg_max_queue_size)
      msg_queue.pop();
}

void user::on_write()
{
   // Called when the session successfully sends a message to the
   // user. Maybe usefull for statistics.
}

void user::reset()
{
   // TODO: Verify if we have to perform some cleanup before reseting,
   // like releasing groups.

   id = {};
   session = {};
   msg_queue = std::queue<std::string> {};
}

void user::shutdown()
{
   if (auto s = session.lock()) {
      s->do_close();
      return;
   }

   // Nothing to do, the user is offline.
}


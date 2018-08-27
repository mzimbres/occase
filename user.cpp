#include "user.hpp"

#include <algorithm>

#include "server_session.hpp"

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
   const auto is_empty = msg_queue.empty();
   msg_queue.push(std::move(msg));

   if (is_empty) {
      if (auto s = session.lock()) {
         s->write(msg_queue.front());
         return;
      }

      // TODO: Decide what to do here.
      std::cerr << "Session is expired." << std::endl;
   }
}

void user::on_write()
{
   msg_queue.pop();

   if (msg_queue.empty())
      return; // No more message to send to the client.

   if (auto s = session.lock()) {
      s->write(msg_queue.front());
      return;
   }

   // TODO: Decide what to do here.
   std::cerr << "Session is expired." << std::endl;
}

void user::reset()
{
   // TODO: Verify if we have to perform some cleanup before reseting,
   // like releasing groups.

   id = {};
   session = {};
   msg_queue = std::queue<std::string> {};
}


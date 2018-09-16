#include "group.hpp"

#include "server_session.hpp"

void group::broadcast_msg(std::string msg)
{
   auto begin = std::begin(local_members);
   auto end = std::end(local_members);
   while (begin != end) {
      if (auto s = begin->second.lock()) {
         // The user is online. We can just forward his message.
         // TODO: We incurr here the possibility of sending repeated
         // messages to the user. The scenario is as follows
         // 1. We send a message bellow and it is buffered.
         // 2. The user disconnects before receiving the message.
         // 3. The message are saved on the database.
         // 4. The user reconnects and we read and send him his
         //    messages from the database.
         // 5. We traverse the groups sending him the latest messages
         //    the he missed while he was offline and this message is
         //    between them.
         // This is perhaps unlikely but should be avoided in the
         // future.
         s->send_msg(msg);
         ++begin;
         continue;
      }

      // Removes users that are not online anymore.
      begin = local_members.erase(begin);
   }
}


#include "channel.hpp"

#include "server_session.hpp"

namespace rt
{

void channel::broadcast(std::string const& msg)
{
   //std::cout << "Broadcast size: " << std::size(members) << std::endl;
   auto begin = std::begin(members);
   auto end = std::end(members);
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
         // 5. We traverse the channels sending him the latest messages
         //    that he missed while he was offline and this message is
         //    between them.
         // This is perhaps unlikely but should be avoided in the
         // future.
         //std::cout << "on_group_msg: sending to " << s->get_id()
         //          << " " << msg << std::endl;
         s->send(msg);
         ++begin;
         continue;
      }

      // Removes users that are not online anymore.
      begin = members.erase(begin);
   }
}

void channel::add_member(std::shared_ptr<server_session> s)
{
   // Overwrites any previous session.
   auto const from = s->get_id();
   members[from] = s; // Overwrites any previous session.
}

}


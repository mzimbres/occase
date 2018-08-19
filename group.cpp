#include "group.hpp"

void to_json(json& j, const group_info& g)
{
  j = json{{"title", g.title}, {"description", g.description}};
}

void from_json(const json& j, group_info& g)
{
  g.title = j.at("title").get<std::string>();
  g.description = j.at("description").get<std::string>();
}

void group::broadcast_msg( std::string msg
                         , grow_only_vector<user>& users) const
{
   for (auto const& user : members) {
      users[user.first].send_msg(msg);
   }
}


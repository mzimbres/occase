#include "json_utils.hpp"

#include <string>

std::ostream& operator<<(std::ostream& os, group_info const& info)
{
   os << "   Title:       " << info.title << "\n"
      << "   Description: " << info.description << "\n";

   return os;
}

void to_json(json& j, group_info const& g)
{
  j = json{{"title", g.title}, {"description", g.description}};
}

void from_json(json const& j, group_info& g)
{
  g.title = j.at("title").get<std::string>();
  g.description = j.at("description").get<std::string>();
}


#include "json_utils.hpp"

#include <string>

std::ostream& operator<<(std::ostream& os, group_info const& info)
{
   os << "   Title:       " << info.title << "\n"
      << "   Description: " << info.description << "\n";

   return os;
}

std::ostream& operator<<(std::ostream& os, user_bind const& info)
{
   os << "   Tel:   " << info.tel   << "\n"
      << "   Host:  " << info.host  << "\n"
      << "   Index: " << info.index << "\n";

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

void to_json(json& j, user_bind const& u)
{
  j = json{{"tel", u.tel}, {"host", u.host}, {"index", u.index}};
}

void from_json(json const& j, user_bind& u)
{
  u.tel = j.at("tel").get<std::string>();
  u.host = j.at("host").get<std::string>();
  u.index = j.at("index").get<int>();
}


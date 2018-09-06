#include "json_utils.hpp"

#include <string>

std::ostream& operator<<(std::ostream& os, group_info const& info)
{
   os << "   Title:       " << info.title << "\n"
      << "   Description: " << info.description << "\n";

   return os;
}

std::ostream& operator<<(std::ostream& os, group_bind const& bind)
{
   os << "   Host:  " << bind.host << "\n"
      << "   Index: " << bind.index << "\n";

   return os;
}

std::ostream& operator<<(std::ostream& os, user_bind const& info)
{
   os << "\n"
      << "   Tel:   " << info.tel   << "\n"
      << "   Host:  " << info.host  << "\n"
      << "   Index: " << info.index << "\n";

   return os;
}

void to_json(json& j, group_info const& g)
{
  j = json{{"title", g.title}, {"description", g.description}};
}

void to_json(json& j, group_bind const& g)
{
  j = json{{"host", g.host}, {"index", g.index}};
}

void to_json(json& j, user_bind const& u)
{
  j = json{{"tel", u.tel}, {"host", u.host}, {"index", u.index}};
}

void from_json(json const& j, group_info& g)
{
  g.title = j.at("title").get<std::string>();
  g.description = j.at("description").get<std::string>();
}

void from_json(json const& j, group_bind& g)
{
  g.host = j.at("host").get<std::string>();
  g.index = j.at("index").get<int>();
}

void from_json(json const& j, user_bind& u)
{
  u.tel = j.at("tel").get<std::string>();
  u.host = j.at("host").get<std::string>();
  u.index = j.at("index").get<int>();
}


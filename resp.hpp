#pragma once

#include <vector>
#include <string>
#include <initializer_list>

namespace rt::redis
{

std::size_t get_length(char const* p);

enum class command
{ get
, incrby
, lpop
, lrange
, ping
, rpush
, publish
, set
, subscribe
, unsubscribe
};

std::string
gen_resp_cmd(command cmd, std::initializer_list<std::string> param);

}


#pragma once

#include <vector>
#include <string>
#include <initializer_list>

namespace rt
{

std::size_t get_length(char const* p);

enum class redis_cmd
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
gen_resp_cmd(redis_cmd cmd, std::initializer_list<std::string> param);

}


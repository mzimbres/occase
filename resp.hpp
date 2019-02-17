#pragma once

#include <vector>
#include <string>
#include <initializer_list>

namespace rt::redis
{

// Enum of redis commands in one-to-one correspondence with redis
// documentation.
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

// Assembles strings into a redis command (in resp format).
std::string
gen_resp_cmd(command cmd, std::initializer_list<std::string> param);

}


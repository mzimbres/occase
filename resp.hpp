#pragma once

#include <vector>
#include <string>
#include <initializer_list>

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
, unsolicited // No a redis cmd. Received in subscribe mode.
};

struct redis_req {
   redis_cmd cmd;
   std::string msg;
};

redis_req
gen_resp_cmd(redis_cmd cmd, std::initializer_list<std::string> param);


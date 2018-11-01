#pragma once

#include <vector>
#include <string>

std::size_t get_length(char const* p);

std::string gen_resp_cmd(std::string cmd, std::vector<std::string> param);


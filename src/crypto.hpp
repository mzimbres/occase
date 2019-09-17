#pragma once

#include <string>
#include <random>

namespace rt
{

void init_libsodium();
std::string make_hex_digest(std::string const& input);

std::string
make_hex_digest( std::string const& input
               , std::string const& key);

class pwd_gen {
private:
   std::mt19937 gen;
   std::uniform_int_distribution<int> dist;

public:
   pwd_gen();
   std::string operator()(int size);
};

}


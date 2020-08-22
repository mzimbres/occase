#pragma once

#include <string>
#include <random>

namespace occase
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
   // sep below is a character that is not part of the character set
   // used to generate the random strings and can be used as a
   // separator.
   static constexpr char sep = '-';

   pwd_gen();
   std::string make(int size);
   std::string make_key();
};

}


#include <string>
#include <iostream>

#include <sodium.h>

#include "crypto.hpp"

using namespace rt;

int main()
{
   pwd_gen gen;
   std::cout << gen(crypto_generichash_KEYBYTES) << std::endl;
}


#include <sodium.h>

#include <array>
#include <string>
#include <iostream>
#include <sstream>

//constexpr auto hash_size = crypto_generichash_BYTES;
constexpr auto hash_size = crypto_generichash_BYTES_MIN;
using hash_type = std::array<unsigned char, hash_size>;

constexpr char hextable[] = "0123456789abcdef";

char low_to_char(unsigned char a)
{
    return hextable[a & 0x0f];
}

char high_to_char(unsigned char a)
{
    return hextable[(a & 0xf0) >> 4];
}

std::string hash_to_string(hash_type const& hash)
{
  std::string output;
  output.reserve(2 * std::size(hash));
  for (auto i = 0; i < std::size(hash); ++i) {
    output.push_back(high_to_char(hash[2 * i]));
    output.push_back(low_to_char(hash[2 * i]));
  }

  return output;
}

std::string hash(std::string const input)
{
  auto const* p1 = reinterpret_cast<unsigned char const*>(input.data());

  hash_type hash;
  crypto_generichash(hash.data(), std::size(hash),
     p1, std::size(input), nullptr, 0);

  return hash_to_string(hash);
}

int main(int argc, char* argv[])
{
   if (argc != 2) {
     std::cerr << "Usage: " << argv[0] << " string" << std::endl;
     return 1;
   }

   if (sodium_init() == -1)
      return 1;

   std::cout << "Hash: " << hash(argv[1]) << std::endl;
}


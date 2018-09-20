#include <iostream>

#include "idx_mgr.hpp"

idx_mgr::idx_mgr(int size)
{
   while (size-- != 0)
      avail.push(size);
}

int idx_mgr::allocate() noexcept
{
   if (avail.empty()) {
      std::cout << "allocate: -1" << std::endl;
      return -1;
   }

   auto const i = avail.top();
   avail.pop();
   std::cout << "allocate: " << i << std::endl;
   return i;
}

void idx_mgr::deallocate(int idx)
{
   std::cout << "deallocate: " << idx << std::endl;
   avail.push(idx);
}


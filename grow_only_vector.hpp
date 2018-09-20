#pragma once

#include <stack>

class idx_mgr {
private:
   std::stack<int> avail;

public:
   idx_mgr(int size)
   {
      while (size-- != 0)
         avail.push(size);
   }

   auto allocate() noexcept
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

   void deallocate(int idx)
   {
      std::cout << "deallocate: " << idx << std::endl;
      assert(idx >= 0);
      avail.push(idx);
   }

};


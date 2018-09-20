#pragma once

#include <stack>

class idx_mgr {
private:
   std::stack<int> avail;

public:
   idx_mgr(int size);
   int allocate() noexcept;
   void deallocate(int idx);
};


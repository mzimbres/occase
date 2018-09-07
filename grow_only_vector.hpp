#pragma once

#include <stack>
#include <vector>

// Items will be added in the vector and wont be removed, instead, we
// will push its index in the stack. When a new item is requested we
// will pop one index from the stack and use it, if none is available
// we push_back in the vector.
template <class T>
class grow_only_vector {
public:
   // I do not want an unsigned index type.
   //using size_type = typename std::vector<T>::size_type;
   using size_type = int;
   using reference = typename std::vector<T>::reference;
   using const_reference = typename std::vector<T>::const_reference;

private:
   std::stack<size_type> avail;
   std::vector<T> items;

public:
   grow_only_vector(int size)
   : items(size)
   {
      while (size-- != 0)
         avail.push(size);
   }

   reference operator[](size_type i) noexcept
   { return items[i]; };

   const_reference operator[](size_type i) const noexcept
   { return items[i]; };

   // Returns the index of an element int the group that is free
   // for use.
   auto allocate() noexcept
   {
      if (avail.empty()) {
         std::cout << "allocate: -1" << std::endl;
         return static_cast<size_type>(-1);
      }

      auto i = avail.top();
      avail.pop();
      std::cout << "allocate: " << i << std::endl;
      return i;
   }

   // TODO: Check out if we can make this noexcept since it begins
   // with its maximum size on construction.
   void deallocate(size_type idx) noexcept
   {
      std::cout << "deallocate: " << idx << std::endl;
      assert(idx >= 0);
      avail.push(idx);
   }

   auto is_valid_index(size_type idx) const noexcept
   {
      const auto a = idx >= static_cast<size_type>(0);
      const auto b = idx < static_cast<size_type>(items.size());
      return a && b;
   }
};


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
   reference operator[](size_type i)
   { return items[i]; };

   const_reference operator[](size_type i) const
   { return items[i]; };

   // Returns the index of an element int the group that is free
   // for use.
   auto allocate()
   {
      if (avail.empty()) {
         auto size = items.size();
         items.push_back({});
         return static_cast<size_type>(size);
      }

      auto i = avail.top();
      avail.pop();
      return i;
   }

   void deallocate(size_type idx)
   {
      avail.push(idx);
   }

   auto is_valid_index(size_type idx) const noexcept
   {
      return idx >= 0
          && idx < static_cast<size_type>(items.size());
   }
};



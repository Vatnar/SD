#pragma once

#include "SD/arena.hpp"

template<typename T>
struct ArenaVec {
  T*  data  = nullptr;
  U64 count = 0;
  U64 cap   = 0;

  void push(Arena* arena, const T& item) {
    if (count >= cap) {
      cap         = cap ? cap * 2 : 16;
      T* new_data = arena->push_array<T>(cap);
      for (U64 i = 0; i < count; ++i)
        new_data[i] = data[i];
      data = new_data;
    }
    data[count++] = item;
  }

  void clear() {
    count = 0;
    data  = nullptr;
    cap   = 0;
  }

  T&       operator[](U64 i) { return data[i]; }
  const T& operator[](U64 i) const { return data[i]; }

  T*       begin() { return data; }
  T*       end() { return data + count; }
  const T* begin() const { return data; }
  const T* end() const { return data + count; }
};

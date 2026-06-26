#pragma once

#include <type_traits>

#include <sys/mman.h>

#include "../../../build/release-unity/SD/generated/SD/export.hpp"
#include "core/base.hpp"
#include "core/types.hpp"

template<typename T>
FORCE_INLINE T constexpr align_pow2(T x, T b) {
  return ((x + b - 1) & (~b - 1));
}

consteval U64 kb(U64 n) {
  return n << 10;
}
consteval U64 mb(U64 n) {
  return n << 20;
}
consteval U64 gb(U64 n) {
  return n << 30;
}
consteval U64 tb(U64 n) {
  return n << 40;
}

template<typename T>
FORCE_INLINE T constexpr min(T a, T b) {
  return a < b ? a : b;
}

template<typename T>
FORCE_INLINE T constexpr max(T a, T b) {
  return a > b ? a : b;
}

template<typename T>
FORCE_INLINE T constexpr clamp_top(T a, T x) {
  return min(a, x);
}
template<typename T>
FORCE_INLINE T constexpr clamp_bot(T x, T b) {
  return max(x, b);
}

template<typename T>
FORCE_INLINE T constexpr clamp(T a, T x, T b) {
  if (x < a) {
    return a;
  }
  if (x > b) {
    return b;
  }
  return x;
}


enum class ArenaFlags : U64 {
  NONE        = 0,
  NO_CHAIN    = (1 << 0),
  LARGE_PAGES = (1 << 1),
};
BITMASK_ENUM(ArenaFlags);

struct ArenaParams {
  ArenaFlags         flags                   = ArenaFlags::NONE;
  U64                reserve_size            = mb(64uz);
  U64                commit_size             = kb(64uz);
  void*              optional_backing_buffer = nullptr;
  sd::SourceLocation location                = sd::SourceLocation::current();
  const char*        name                    = nullptr;
};


struct Arena;
struct Temp {
  Arena* arena;
  U64    pos;

  void end(this Temp temp);
};

struct SD_EXPORT Arena {
  Arena*             prev;
  Arena*             current;
  ArenaFlags         flags;
  U64                commit_size;
  U64                reserve_size;
  U64                base_position;
  U64                position;
  U64                committed;
  U64                reserved;
  sd::SourceLocation location;
  const char*        name;

  void* push(this Arena& arena, U64 size, U64 align, bool zero);
  U64   pos(this Arena& arena);
  void  pop_to(this Arena& arena, U64 pos);
  void  clear(this Arena& arena);
  void  pop(this Arena& arena, U64 amount);

  [[nodiscard]] Temp temp_begin(this Arena& arena);

  //~ helpers
  template<typename T>
  T* push_array_no_zero_aligned(this Arena& arena, U64 count, U64 align) {
    return static_cast<T*>(arena.push(sizeof(T) * count, align, false));
  }
  template<typename T>
  T* push_array_aligned(this Arena& arena, U64 count, U64 align) {
    return static_cast<T*>(arena.push(sizeof(T) * count, align, true));
  }
  template<typename T>
  T* push_array_no_zero(this Arena& arena, U64 count) {
    return arena.push_array_no_zero_aligned<T>(count, max(8uz, alignof(T)));
  }
  template<typename T>
  T* push_array(this Arena& arena, U64 count) {
    return arena.push_array_aligned<T>(count, max(8uz, alignof(T)));
  }
};

SD_EXPORT Arena* arena_alloc(ArenaParams params = {});
SD_EXPORT void   arena_release(Arena* arena);

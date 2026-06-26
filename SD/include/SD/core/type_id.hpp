#pragma once

#include "types.hpp"

template<typename T>
consteval U64 type_id_of() {
  constexpr std::string_view name = __PRETTY_FUNCTION__;
  U64                        hash = 14695981039346656037ULL;
  for (USize i = 0; i < name.size(); ++i) {
    hash ^= static_cast<U8>(name[i]);
    hash *= 1099511628211ULL;
  }
  return hash;
}

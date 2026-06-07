#pragma once
#include <VLA/Vector.hpp>

#define IM_VEC2_CLASS_EXTRA                                     \
  constexpr ImVec2(const VLA::Vector2f& v) : x(v[0]), y(v[1]) { \
  }                                                             \
  operator VLA::Vector2f() const {                              \
    return {x, y};                                              \
  }

#define IM_VEC4_CLASS_EXTRA                                                       \
  constexpr ImVec4(const VLA::Vector4f& v) : x(v[0]), y(v[1]), z(v[2]), w(v[3]) { \
  }                                                                               \
  operator VLA::Vector4f() const {                                                \
    return {x, y, z, w};                                                          \
  }

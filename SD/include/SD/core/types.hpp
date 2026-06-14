// TODO(docs): Add file-level Doxygen header
//   - @file types.hpp
//   - @brief Engine type aliases
//   - Rationale for using custom type aliases (clarity, consistency)
#pragma once
#include <cstdint>
#include <limits>
using I8  = std::int8_t;
using I16 = std::int16_t;
using I32 = std::int32_t;
using I64 = std::int64_t;

using U8  = std::uint8_t;
using U16 = std::uint16_t;
using U32 = std::uint32_t;
using U64 = std::uint64_t;

using USize = std::size_t;

using F32 = float;
using F64 = double;

#define LOCAL_PERSIST static

template<typename T>
constexpr T g_type_max = std::numeric_limits<T>::max();

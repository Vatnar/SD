#pragma once

#include "SD/core/logging.hpp"

#if defined(_MSC_VER)
#define TRAP() __debugbreak()
#elif defined(__GNUC__) || defined(__clang__)
#define TRAP() __builtin_trap()
#else
#error Unknown trap intrinsic for compiler.
#endif
#define ASSERT_ALWAYS(x)                                                                 \
  do {                                                                                   \
    if (!(x)) {                                                                          \
      ::sd::log::engine::critical("ASSERT FAILED: {} at {}:{}", #x, __FILE__, __LINE__); \
      TRAP();                                                                            \
    }                                                                                    \
  } while (0)

#ifdef SD_DEBUG
#define ASSERT(x) ASSERT_ALWAYS(x)
#else
#define ASSERT(x) (void)(x)
#endif
#define INVALID_PATH    ASSERT(!"Invalid Path!")
#define NOT_IMPLEMENTED ASSERT(!"Not Implemented!")
#define NO_OP           ((void)0)

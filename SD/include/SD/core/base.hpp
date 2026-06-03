#pragma once

#include <spdlog/spdlog.h>

#include "SD/core/logging.hpp"

#if defined(_MSC_VER)
#define TRAP() __debugbreak()
#elif defined(__GNUC__) || defined(__clang__)
#define TRAP() __builtin_trap()
#else
#error Unknown trap intrinsic for compiler.
#endif
#define ASSERT_ALWAYS(x)                                                      \
  do {                                                                        \
    if (!(x)) {                                                               \
      spdlog::critical("ASSERT FAILED: {} at {}:{}", #x, __FILE__, __LINE__); \
      TRAP();                                                                 \
    }                                                                         \
  } while (0)

#ifndef NDEBUG
#define ASSERT(x) ASSERT_ALWAYS(x)
#else
#define ASSERT(x) (void)(x)
#endif
#define INVALID_PATH    ASSERT(!"Invalid Path!")
#define NOT_IMPLEMENTED ASSERT(!"Not Implemented!")
#define NO_OP           ((void)0)


namespace sd {
// TODO(docs): Document Abort() overloads
//   - Explain when to use vs exceptions
//   - Note about spdlog shutdown behavior
//   - Thread-safety considerations
/**
 * Terminates the engine and prints following fatal message
 * @param message
 */
[[noreturn]] inline void engine_abort(const std::string& message) {
  log::engine::critical("Fatal error: {}", message);
  spdlog::shutdown();
  std::abort();
}
[[noreturn]] inline void engine_abort() {
  spdlog::shutdown();
  std::abort();
}

} // namespace sd

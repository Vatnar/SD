#pragma once
#include <string_view>
#include <x86intrin.h>

#include "core/logging.hpp"
#include "core/types.hpp"

namespace sd {

struct Profile {
  u64         start{};
  const char* name{};

  Profile(const char* name) : name{name} {
    _mm_lfence();
    start = __rdtsc();
  }

  ~Profile() noexcept {
    u32 aux;
    u64 end = __rdtscp(&aux);
    _mm_lfence();

    // TODO: consider how long debug takes
    // _mm_lfence();
    // u64 start2 = __rdtsc();
    // sd::log::engine::profiler::debug("{}: {} cycles", name, end - start);
    printf("[profiler] [debug] %s: %lu cycles\n", name, end - start);
    // u64 end2 = __rdtscp(&aux);
    // _mm_lfence();
    // sd::log::engine::profiler::debug("logging: {} cycles", end2 - start2);
  }

  Profile(const Profile&)            = delete;
  Profile& operator=(const Profile&) = delete;
  Profile(Profile&&)                 = delete;
  Profile& operator=(Profile&&)      = delete;
};
} // namespace sd

#define SD_CONCAT_INNER(a, b) a##b
#define SD_CONCAT(a, b)       SD_CONCAT_INNER(a, b)

#define PROFILE(name)                               \
  ::sd::Profile SD_CONCAT(_sd_profile_, __LINE__) { \
    name                                            \
  }
#define PROFILE_FUNCTION() PROFILE(__func__)

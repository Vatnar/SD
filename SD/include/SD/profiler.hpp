#pragma once
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string_view>
#include <x86intrin.h>

#include "core/logging.hpp"
#include "core/types.hpp"

namespace sd {

namespace detail {

inline U64 get_cpu_mhz() {
  static U64 cached_mhz = [] {
    U64 mhz = 0;
    if (FILE* f = fopen("/proc/cpuinfo", "r")) {
      char buf[256];
      while (fgets(buf, sizeof(buf), f)) {
        if (const char* p = std::strstr(buf, "cpu MHz")) {
          mhz = static_cast<U64>(std::strtod(p + 7, nullptr));
          break;
        }
      }
      fclose(f);
    }
    return mhz ? mhz : 3000;
  }();
  return cached_mhz;
}

} // namespace detail

struct Profile {
  U64         start{};
  const char* name{};

  Profile() = default;

  Profile(const char* name) : name{name} {
    _mm_lfence();
    start = __rdtsc();
  }

  ~Profile() noexcept {
    if (!name)
      return;
    U32 aux;
    U64 end = __rdtscp(&aux);
    _mm_lfence();
    U64 elapsed = end - start;
    U64 cur_mhz = detail::get_cpu_mhz();
    sd::log::engine::profiler::debug(
        "{}: {} cycles, {:.2f} ms @ 3GHz, {:.2f} ms @ {} MHz (current)",
        name,
        elapsed,
        elapsed / 3'000'000.0,
        elapsed / (cur_mhz * 1000.0),
        cur_mhz);
  }

  void begin(const char* new_name) {
    name = new_name;
    _mm_lfence();
    start = __rdtsc();
  }

  U64 end() {
    U32 aux;
    U64 end_tsc = __rdtscp(&aux);
    _mm_lfence();
    U64 elapsed = end_tsc - start;
    U64 cur_mhz = detail::get_cpu_mhz();

    sd::log::engine::profiler::debug(
        "{}: {} cycles, {:.2f} ms @ 3GHz, {:.2f} ms @ {} MHz (current)",
        name,
        elapsed,
        elapsed / 3'000'000.0,
        elapsed / (cur_mhz * 1000.0),
        cur_mhz);
    name = nullptr;
    return elapsed;
  }

  Profile(const Profile&)            = delete;
  Profile& operator=(const Profile&) = delete;
  Profile(Profile&&)                 = delete;
  Profile& operator=(Profile&&)      = delete;
};

inline thread_local Profile _sd_active_profile{};
} // namespace sd

#define SD_CONCAT_INNER(a, b) a##b
#define SD_CONCAT(a, b)       SD_CONCAT_INNER(a, b)

#define PROFILE(name)                               \
  ::sd::Profile SD_CONCAT(_sd_profile_, __LINE__) { \
    name                                            \
  }
#define PROFILE_FUNCTION()  PROFILE(__func__)

#define PROFILE_START(name) ::sd::_sd_active_profile.begin(name)
#define PROFILE_END()       ::sd::_sd_active_profile.end()

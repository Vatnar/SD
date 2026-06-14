/**
 * @file PerformanceLayer.hpp
 * @brief x86-only performance monitoring layer using RDTSC
 *
 * @note This layer requires x86_64 architecture. Not portable to ARM.
 */
#pragma once
#include <chrono>

#if defined(__x86_64__) || defined(_M_X64)
#include <x86intrin.h>
#else
#error "PerformanceLayer requires x86_64 (__rdtsc). Consider using std::chrono instead."
#endif

#include "SD/core/Layer.hpp"
#include "SD/core/logging.hpp"
namespace sd {
class PerformanceLayer : public Layer {
public:
  explicit PerformanceLayer() : Layer("Performance layer") { m_last_cycles = __rdtsc(); }

  void on_update(const float dt) override {
    m_frame_count++;
    m_time_accumulator += dt;

    U64 current_cycles{__rdtsc()};
    if (m_last_cycles != 0) {
      m_cycle_accumulator += (current_cycles - m_last_cycles);
    }
    m_last_cycles = current_cycles;

    if (m_time_accumulator >= 1.0f) {
      double fps         = m_frame_count / static_cast<double>(m_time_accumulator);
      double computeTime = static_cast<double>(m_time_accumulator) - m_sleep_time_accumulator;
      double msPerFrame  = (computeTime * 1000.0) / m_frame_count;
      double avgCycles =
          static_cast<double>(m_cycle_accumulator - m_sleep_cycle_accumulator) / m_frame_count;


      log::engine::info("FPS: {:.2f}, Avg ms: {:.2f}, Avg cycles: {:.0f}",
                        fps,
                        msPerFrame,
                        avgCycles);

      m_frame_count             = 0;
      m_time_accumulator        = 0.0f;
      m_cycle_accumulator       = 0;
      m_sleep_time_accumulator  = 0.0f;
      m_sleep_cycle_accumulator = 0;
    }
  }

  void begin_sleep() {
    m_sleep_start_cycles = __rdtsc();
    m_sleep_start_time   = std::chrono::high_resolution_clock::now();
  }

  void end_sleep() {
    auto now = std::chrono::high_resolution_clock::now();
    m_sleep_cycle_accumulator += __rdtsc() - m_sleep_start_cycles;
    m_sleep_time_accumulator += std::chrono::duration<float>(now - m_sleep_start_time).count();
  }

private:
  U32   m_frame_count       = 0;
  float m_time_accumulator  = 0.0f;
  U64   m_cycle_accumulator = 0;
  U64   m_last_cycles       = 0;

  float                                                       m_sleep_time_accumulator  = 0;
  U64                                                         m_sleep_cycle_accumulator = 0;
  U64                                                         m_sleep_start_cycles      = 0;
  std::chrono::time_point<std::chrono::high_resolution_clock> m_sleep_start_time;
};
} // namespace sd

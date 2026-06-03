// TODO(docs): Add file-level Doxygen header
//   - @file FrameTimer.hpp
//   - @brief Frame timing and fixed timestep management
//   - Explain the fixed timestep pattern
//   - GPU/CPU time tracking
#pragma once

#include <GLFW/glfw3.h>

#include "SD/export.hpp"

namespace sd {

// TODO(docs): Document FrameTimer class
//   - Purpose: Manages frame timing, fixed timestep accumulation
//   - Usage pattern: Begin -> BeginWork -> EndWork -> ConsumeFixedStep (loop)
//   - Frame time clamping (max 0.25s)
//   - GPU wait time tracking
//   - Example game loop using FrameTimer
/// Tracks frame timing, fixed timestep accumulation, and CPU work time.
class SD_EXPORT FrameTimer {
public:
  void begin() {
    double now   = glfwGetTime();
    m_frame_time = now - m_last_time;
    m_last_time  = now;

    if (m_frame_time > 0.25)
      m_frame_time = 0.25;
    m_accumulator += m_frame_time;
  }

  void begin_work() { m_work_start = glfwGetTime(); }

  void end_work() {
    m_frame_work_time = static_cast<float>(glfwGetTime() - m_work_start) - m_gpu_wait_time;
    m_gpu_wait_time   = 0.0f;
  }

  /// Returns true if a fixed step should run. Call in a loop.
  bool consume_fixed_step() {
    if (m_accumulator >= m_fixed_time_step) {
      m_accumulator -= m_fixed_time_step;
      return true;
    }
    return false;
  }

  void add_gpu_wait_time(float t) { m_gpu_wait_time += t; }

  // Accessors
  [[nodiscard]] float  get_frame_time() const { return static_cast<float>(m_frame_time); }
  [[nodiscard]] float  get_frame_work_time() const { return m_frame_work_time; }
  [[nodiscard]] double get_fixed_time_step() const { return m_fixed_time_step; }
  void                 set_fixed_time_step(double step) { m_fixed_time_step = step; }

private:
  double m_last_time       = 0.0;
  double m_accumulator     = 0.0;
  double m_fixed_time_step = 1.0 / 60.0;
  double m_work_start      = 0.0;
  float  m_frame_work_time = 0.0f;
  float  m_gpu_wait_time   = 0.0f;
  double m_frame_time      = 0.0;
};

} // namespace sd

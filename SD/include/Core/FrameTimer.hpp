#pragma once

#include <GLFW/glfw3.h>

namespace SD {

/// Tracks frame timing, fixed timestep accumulation, and CPU work time.
class FrameTimer {
public:
  void Begin() {
    double now = glfwGetTime();
    mFrameTime = now - mLastTime;
    mLastTime = now;

    if (mFrameTime > 0.25)
      mFrameTime = 0.25;
    mAccumulator += mFrameTime;
  }

  void BeginWork() { mWorkStart = glfwGetTime(); }

  void EndWork() {
    mFrameWorkTime = static_cast<float>(glfwGetTime() - mWorkStart) - mGpuWaitTime;
    mGpuWaitTime = 0.0f;
  }

  /// Returns true if a fixed step should run. Call in a loop.
  bool ConsumeFixedStep() {
    if (mAccumulator >= mFixedTimeStep) {
      mAccumulator -= mFixedTimeStep;
      return true;
    }
    return false;
  }

  void AddGpuWaitTime(float t) { mGpuWaitTime += t; }

  // Accessors
  [[nodiscard]] float GetFrameTime() const { return static_cast<float>(mFrameTime); }
  [[nodiscard]] float GetFrameWorkTime() const { return mFrameWorkTime; }
  [[nodiscard]] double GetFixedTimeStep() const { return mFixedTimeStep; }
  void SetFixedTimeStep(double step) { mFixedTimeStep = step; }

private:
  double mLastTime = 0.0;
  double mAccumulator = 0.0;
  double mFixedTimeStep = 1.0 / 60.0;
  double mWorkStart = 0.0;
  float mFrameWorkTime = 0.0f;
  float mGpuWaitTime = 0.0f;
  double mFrameTime = 0.0;
};

} // namespace SD

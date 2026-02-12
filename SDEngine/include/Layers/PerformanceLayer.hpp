#pragma once
#include <chrono>
#include <spdlog/spdlog.h>
// TODO: Only works on x86, need some backup if not on x86
// TODO: Use std::chrono or platform-independent high-resolution timer instead of __rdtsc for portability
#include <x86intrin.h>

#include "Core/Layer.hpp"

class PerformanceLayer : public Layer {
public:
  PerformanceLayer() { mLastCycles = __rdtsc(); }

  void OnUpdate(float dt) override {
    mFrameCount++;
    mTimeAccumulator += dt;

    uint64_t currentCycles = __rdtsc();
    if (mLastCycles != 0) {
      mCycleAccumulator += (currentCycles - mLastCycles);
    }
    mLastCycles = currentCycles;

    if (mTimeAccumulator >= 1.0f) {
      double fps = mFrameCount / static_cast<double>(mTimeAccumulator);
      double computeTime = static_cast<double>(mTimeAccumulator) - mSleepTimeAccumulator;
      double msPerFrame = (computeTime * 1000.0) / mFrameCount;
      double avgCycles =
          static_cast<double>(mCycleAccumulator - mSleepCycleAccumulator) / mFrameCount;


      if (const auto logger = spdlog::get("engine"))
        logger->info("FPS: {:.2f}, Avg ms: {:.2f}, Avg cycles: {:.0f}", fps, msPerFrame, avgCycles);

      mFrameCount = 0;
      mTimeAccumulator = 0.0f;
      mCycleAccumulator = 0;
      mSleepTimeAccumulator = 0.0f;
      mSleepCycleAccumulator = 0;
    }
  }

  void BeginSleep() {
    mSleepStartCycles = __rdtsc();
    mSleepStartTime = std::chrono::high_resolution_clock::now();
  }

  void EndSleep() {
    auto now = std::chrono::high_resolution_clock::now();
    mSleepCycleAccumulator += __rdtsc() - mSleepStartCycles;
    mSleepTimeAccumulator += std::chrono::duration<float>(now - mSleepStartTime).count();
  }

private:
  uint32_t mFrameCount = 0;
  float mTimeAccumulator = 0.0f;
  uint64_t mCycleAccumulator = 0;
  uint64_t mLastCycles = 0;

  float mSleepTimeAccumulator = 0;
  uint64_t mSleepCycleAccumulator = 0;
  uint64_t mSleepStartCycles = 0;
  std::chrono::time_point<std::chrono::high_resolution_clock> mSleepStartTime;
};

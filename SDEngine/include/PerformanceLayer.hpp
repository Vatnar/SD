#pragma once
#include "Layer.hpp"
#include <spdlog/spdlog.h>
#include <chrono>
#include <x86intrin.h>

class PerformanceLayer : public Layer
{
public:
    PerformanceLayer()
    {
        mLastCycles = __rdtsc();
    }

    void OnUpdate(float dt) override
    {
        mFrameCount++;
        mTimeAccumulator += dt;

        uint64_t currentCycles = __rdtsc();
        if (mLastCycles != 0)
        {
            mCycleAccumulator += (currentCycles - mLastCycles);
        }
        mLastCycles = currentCycles;

        if (mTimeAccumulator >= 1.0f)
        {
            double fps        = mFrameCount / static_cast<double>(mTimeAccumulator);
            double msPerFrame = (mTimeAccumulator * 1000.0) / mFrameCount;
            double avgCycles  = static_cast<double>(mCycleAccumulator) / mFrameCount;

            auto logger = spdlog::get("engine");
            if (logger)
                logger->info("FPS: {:.2f}, Avg ms: {:.2f}, Avg cycles: {:.0f}", fps, msPerFrame, avgCycles);

            mFrameCount       = 0;
            mTimeAccumulator  = 0.0f;
            mCycleAccumulator = 0;
        }
    }

private:
    uint32_t mFrameCount       = 0;
    float    mTimeAccumulator  = 0.0f;
    uint64_t mCycleAccumulator = 0;
    uint64_t mLastCycles       = 0;
};
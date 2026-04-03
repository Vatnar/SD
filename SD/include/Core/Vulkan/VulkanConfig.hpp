// TODO(docs): Add file-level Doxygen header
//   - @file VulkanConfig.hpp
//   - @brief Vulkan configuration macros and constants
//   - Explain the VK_HPP_* macros and why they're needed
#pragma once

#ifndef VULKAN_HPP_DISPATCH_LOADER_DYNAMIC
#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#endif
#ifndef VULKAN_HPP_USE_STD_EXPECTED
#define VULKAN_HPP_USE_STD_EXPECTED 1
#endif
#ifndef VULKAN_HPP_NO_EXCEPTIONS
#define VULKAN_HPP_NO_EXCEPTIONS 1
#endif
#include <vulkan/vulkan.hpp>

// TODO(docs): Document MAX_FRAMES_IN_FLIGHT
//   - Purpose: Frame overlap for CPU-GPU parallelism
//   - Trade-offs (higher = more latency, lower = more CPU wait)
//   - How it affects synchronization
namespace SD {
constexpr int MAX_FRAMES_IN_FLIGHT = 2;
}

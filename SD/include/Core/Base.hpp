// TODO(docs): Add file-level Doxygen header
//   - @file Base.hpp
//   - @brief Core engine utilities (abort, Vulkan helpers, math)
//   - Note this is a "catch-all" for low-level utilities
#pragma once

#include <concepts>
#include <format>
#include <iostream>
#include <source_location>
#include <spdlog/spdlog.h>
#include <vulkan/vulkan.hpp>

#include "Core/types.hpp"

namespace SD {

/**
 * @namespace Math
 * @brief Math utilities
 */
namespace Math {
// TODO(docs): Document log2_int - explain use case, constraints (what happens with 0)
consteval usize log2_int(std::unsigned_integral auto n) {
  // log2(n) = bit_width(n) - 1
  return n == 0 ? 0 : std::bit_width(n) - 1;
}
} // namespace Math

// TODO(docs): Document Abort() overloads
//   - Explain when to use vs exceptions
//   - Note about spdlog shutdown behavior
//   - Thread-safety considerations
/**
 * Terminates the engine and prints following fatal message
 * @param message
 */
[[noreturn]] inline void Abort(const std::string& message) {
  if (const auto logger = spdlog::get("engine")) {
    logger->critical("Fatal error: {}", message);
    spdlog::shutdown();
  } else {
    std::cerr << "Fatal error: " << message << '\n';
  }
  std::abort();
}
[[noreturn]] inline void Abort() {
  std::abort();
}

// TODO(docs): Document CheckVulkanRes and CheckVulkanResVal
//   - Explain the pattern for Vulkan error handling
//   - When to use each function
//   - Example usage patterns
inline void CheckVulkanRes(vk::Result result, std::string_view message,
                           std::source_location loc = std::source_location::current()) {
  if (result != vk::Result::eSuccess) {
    auto p =
        std::format("{}:{} {}: {}", loc.file_name(), loc.line(), message, vk::to_string(result));
    Abort(p);
  }
}

template<typename T>
auto CheckVulkanResVal(T&& result, std::string_view message,
                       std::source_location loc = std::source_location::current()) {
  // Try to detect if it's a vk::ResultValue or something like std::expected
  if constexpr (requires { result.result; }) {
    CheckVulkanRes(result.result, message, loc);
    return std::move(result.value);
  } else if constexpr (requires { result.has_value(); }) {
    if (!result.has_value()) {
      CheckVulkanRes(result.error(), message, loc);
    }
    return std::move(result.value());
  } else {
    // Fallback or assume it's already a value (though this shouldn't happen with this name)
    return std::forward<T>(result);
  }
}

inline u32 FindMemoryType(const vk::PhysicalDevice& physicalDevice, u32 typeFilter,
                          vk::MemoryPropertyFlags properties) {
  VkPhysicalDeviceMemoryProperties memProperties;
  vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);
  for (u32 i = 0; i < memProperties.memoryTypeCount; i++) {
    if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags &
                                    static_cast<VkMemoryPropertyFlags>(properties)) ==
                                       static_cast<VkMemoryPropertyFlags>(properties))
      return i;
  }
  Abort("Failed to find memory type");
}

} // namespace SD

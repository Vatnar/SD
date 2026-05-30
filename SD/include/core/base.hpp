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

#include "core/logging.hpp"
#include "core/types.hpp"

namespace sd {

/**
 * @namespace math
 * @brief Math utilities
 */
namespace math {
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
[[noreturn]] inline void engine_abort(const std::string& message) {
  log::engine::critical("Fatal error: {}", message);
  spdlog::shutdown();
  std::abort();
}
[[noreturn]] inline void engine_abort() {
  spdlog::shutdown();
  std::abort();
}

// TODO(docs): Document CheckVulkanRes and CheckVulkanResVal
//   - Explain the pattern for Vulkan error handling
//   - When to use each function
//   - Example usage patterns
inline void check_vulkan_res(vk::Result result, std::string_view message,
                           std::source_location loc = std::source_location::current()) {
  if (result != vk::Result::eSuccess) {
    auto p =
        std::format("{}:{} {}: {}", loc.file_name(), loc.line(), message, vk::to_string(result));
    engine_abort(p);
  }
}

template<typename T>
auto check_vulkan_res_val(T&& result, std::string_view message,
                       std::source_location loc = std::source_location::current()) {
  // Try to detect if it's a vk::ResultValue or something like std::expected
  if constexpr (requires { result.result; }) {
    check_vulkan_res(result.result, message, loc);
    return std::move(result.value);
  } else if constexpr (requires { result.has_value(); }) {
    if (!result.has_value()) {
      check_vulkan_res(result.ERROR(), message, loc);
    }
    return std::move(*result);
  } else {
    // Fallback or assume it's already a value (though this shouldn't happen with this name)
    return std::forward<T>(result);
  }
}

inline u32 find_memory_type(const vk::PhysicalDevice& physical_device, u32 type_filter,
                          vk::MemoryPropertyFlags properties) {
  vk::PhysicalDeviceMemoryProperties mem_properties = physical_device.getMemoryProperties();
  for (u32 i = 0; i < mem_properties.memoryTypeCount; i++) {
    if ((type_filter & (1 << i)) &&
        (mem_properties.memoryTypes[i].propertyFlags & properties) == properties)
      return i;
  }
  engine_abort("Failed to find memory type");
}

} // namespace SD

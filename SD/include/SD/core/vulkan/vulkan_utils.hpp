#pragma once
#include <source_location>

namespace sd {

// TODO(docs): Document CheckVulkanRes and CheckVulkanResVal
//   - Explain the pattern for Vulkan error handling
//   - When to use each function
//   - Example usage patterns
inline void check_vulkan_res(vk::Result result, std::string_view message,
                             std::source_location loc = std::source_location::current()) {
  if (result != vk::Result::eSuccess) {
    log::engine::critical("{}:{} {}: {}", loc.file_name(), loc.line(), message,
                          vk::to_string(result));
    // TODO: why doesnt the vk::to_string(result) work correctly, it shjould return the correct
    // string
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
  log::engine::critical("Failed to find memory type");
  return {};
}

} // namespace sd

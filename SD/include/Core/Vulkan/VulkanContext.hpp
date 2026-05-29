// TODO(docs): Add file-level Doxygen header
//   - @file VulkanContext.hpp
//   - @brief Vulkan device and instance management
//   - Note about class being too large - refactoring planned
//   - Relationship to VulkanWindow and VulkanRenderer
#pragma once

#include <vector>

#include "Core/GlfwContext.hpp"
#include "Core/Window.hpp"
#include "VulkanConfig.hpp"
#include <vk_mem_alloc.h>

namespace sd {
// TODO(docs): Document VulkanContext class thoroughly
//   - Purpose: Manages Vulkan instance, physical device, logical device, and VMA allocator
//   - Initialization order and dependencies
//   - Feature enablement (Vulkan 1.2, 1.3 features)
//   - Queue setup and family indices
//   - Memory management with VMA
//   - Note about refactoring into smaller classes
class VulkanContext {
public:
  explicit VulkanContext(const GlfwContext& glfw_ctx);
  void init(const Window& window);
  ~VulkanContext();

  vk::UniqueInstance& get_instance();
  std::vector<const char*>& get_device_extensions();
  vk::PhysicalDevice& get_physical_device();
  vk::UniqueDevice& get_vulkan_device();
  vk::PhysicalDeviceFeatures2& get_features2();
  vk::PhysicalDeviceVulkan12Features& get_features12();
  vk::PhysicalDeviceVulkan13Features& get_features13();
  VmaAllocator get_vma_allocator() const { return m_allocator; }

  [[nodiscard]] u32 get_graphics_family_index() const;
  [[nodiscard]] vk::Queue get_graphics_queue() const;
  [[nodiscard]] vk::SurfaceFormatKHR get_surface_format() const { return m_surface_format; }
  bool is_initialized() { return m_instance.get() && m_vulkan_device && m_phys_dev; }


private:
  vk::UniqueInstance create_vulkan_application_instance();
  void setup_queues(vk::SurfaceKHR surface);
  void setup_device_extensions();
  void create_vulkan_device();


  const GlfwContext& m_glfw_ctx;
  vk::UniqueInstance m_instance;
  vk::PhysicalDevice m_phys_dev;
  vk::UniqueDevice m_vulkan_device;

  vk::Queue m_graphics_queue;
  u32 m_graphics_family_index{};
  vk::SurfaceFormatKHR m_surface_format;


  std::vector<const char*> m_device_exts;
  PFN_vkGetInstanceProcAddr m_vk_get_instance_proc_addr = nullptr;
  PFN_vkGetDeviceProcAddr m_vk_get_device_proc_addr = nullptr;
  VmaAllocator m_allocator = VK_NULL_HANDLE;
  vk::PhysicalDeviceFeatures2 m_features2;
  vk::PhysicalDeviceVulkan12Features m_features12;
  vk::PhysicalDeviceVulkan13Features m_features13;
};
} // namespace SD

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

namespace SD {
// TODO(docs): Document VulkanContext class thoroughly
//   - Purpose: Manages Vulkan instance, physical device, logical device, and VMA allocator
//   - Initialization order and dependencies
//   - Feature enablement (Vulkan 1.2, 1.3 features)
//   - Queue setup and family indices
//   - Memory management with VMA
//   - Note about refactoring into smaller classes
class VulkanContext {
public:
  explicit VulkanContext(const GlfwContext& glfwCtx);
  void Init(const Window& window);
  ~VulkanContext();

  vk::UniqueInstance& GetInstance();
  std::vector<const char*>& GetDeviceExtensions();
  vk::PhysicalDevice& GetPhysicalDevice();
  vk::UniqueDevice& GetVulkanDevice();
  vk::PhysicalDeviceFeatures2& GetFeatures2();
  vk::PhysicalDeviceVulkan12Features& GetFeatures12();
  vk::PhysicalDeviceVulkan13Features& GetFeatures13();
  VmaAllocator GetVmaAllocator() const { return mAllocator; }

  [[nodiscard]] u32 GetGraphicsFamilyIndex() const;
  [[nodiscard]] vk::Queue GetGraphicsQueue() const;
  [[nodiscard]] vk::SurfaceFormatKHR GetSurfaceFormat() const { return mSurfaceFormat; }
  bool IsInitialized() { return mInstance.get() && mVulkanDevice && mPhysDev; }


private:
  vk::UniqueInstance CreateVulkanApplicationInstance();
  void SetupQueues(vk::SurfaceKHR surface);
  void SetupDeviceExtensions();
  void CreateVulkanDevice();


  const GlfwContext& mGlfwCtx;
  vk::UniqueInstance mInstance;
  vk::PhysicalDevice mPhysDev;
  vk::UniqueDevice mVulkanDevice;

  vk::Queue mGraphicsQueue;
  u32 mGraphicsFamilyIndex{};
  vk::SurfaceFormatKHR mSurfaceFormat;


  std::vector<const char*> mDeviceExts;
  PFN_vkGetInstanceProcAddr mVkGetInstanceProcAddr = nullptr;
  PFN_vkGetDeviceProcAddr mVkGetDeviceProcAddr = nullptr;
  VmaAllocator mAllocator = VK_NULL_HANDLE;
  vk::PhysicalDeviceFeatures2 mFeatures2;
  vk::PhysicalDeviceVulkan12Features mFeatures12;
  vk::PhysicalDeviceVulkan13Features mFeatures13;
};
} // namespace SD

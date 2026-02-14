#pragma once

#include <vector>

#include "Core/GlfwContext.hpp"
#include "Core/LayerList.hpp"
#include "Core/Window.hpp"
#include "VulkanConfig.hpp"


// TODO: This class is doing too much. Consider splitting it into smaller, more focused classes
// (e.g., Device, Swapchain, PipelineManager)
/**
 * @brief RAII class for managing the Vulkan Context.
 * @details Will be split up since its absolutely humongous, perhaps, Device, Swapchain,
 * PipelineManager
 */
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

  [[nodiscard]] uint32_t GetGraphicsFamilyIndex() const;
  [[nodiscard]] vk::Queue GetGraphicsQueue() const;
  [[nodiscard]] vk::SurfaceFormatKHR GetSurfaceFormat() const { return mSurfaceFormat; }
  // TODO: Minimize these raw getters. Instead, provide higher-level functions that perform actions


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
  uint32_t mGraphicsFamilyIndex{};
  vk::SurfaceFormatKHR mSurfaceFormat;


  std::vector<const char*> mDeviceExts;
  vk::PhysicalDeviceFeatures2 mFeatures2;
  vk::PhysicalDeviceVulkan12Features mFeatures12;
  vk::PhysicalDeviceVulkan13Features mFeatures13;
};

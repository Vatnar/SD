#pragma once

#include <vector>

#include "Core/GlfwContext.hpp"
#include "Core/LayerList.hpp"
#include "Core/VulkanConfig.hpp"
#include "Core/Window.hpp"

// TODO: Consider creating a "FrameData" struct to hold all per-frame resources (semaphores, fences,
// command buffers, etc.)
struct FrameSync {
  vk::UniqueSemaphore imageAcquired;
  vk::UniqueFence inFlight;
};

struct SwapchainSync {
  vk::UniqueSemaphore renderComplete;
};

// TODO: This class is doing too much. Consider splitting it into smaller, more focused classes
// (e.g., Device, Swapchain, PipelineManager)
/**
 * @brief RAII class for managing the Vulkan Context.
 * @details Will be split up since its absolutely humongous, perhaps, Device, Swapchain,
 * PipelineManager
 */
class VulkanContext {
public:
  VulkanContext(const GlfwContext& glfwCtx, const Window& window, int maxFramesInFlight);
  ~VulkanContext();

  /**
   * @return Max frames in flight, set in constructor.
   */
  [[nodiscard]] int GetMaxFramesInFlight() const { return mMaxFramesInFlight; }

  FrameSync& GetFrameSync(const uint32_t currentFrame) { return mFrameSyncs[currentFrame]; }
  SwapchainSync& GetSwapchainSync(const uint32_t imageIndex) { return mSwapchainSyncs[imageIndex]; }

  void RecreateSwapchain(LayerList& layers);

  vk::UniqueInstance& GetInstance() { return mInstance; }
  std::vector<const char*>& GetDeviceExtensions() { return mDeviceExts; }
  vk::PhysicalDevice& GetPhysicalDevice() { return mPhysDev; }
  vk::UniqueDevice& GetVulkanDevice() { return mVulkanDevice; }
  vk::PhysicalDeviceFeatures2& GetFeatures2() { return mFeatures2; }
  vk::PhysicalDeviceVulkan12Features& GetFeatures12() { return mFeatures12; }
  vk::PhysicalDeviceVulkan13Features& GetFeatures13() { return mFeatures13; }
  [[nodiscard]] uint32_t GetGraphicsFamilyIndex() const { return mGraphicsFamilyIndex; }
  [[nodiscard]] vk::Queue GetGraphicsQueue() const { return mGraphicsQueue; }
  vk::UniqueSwapchainKHR& GetSwapchain() { return mSwapchain; }
  [[nodiscard]] const std::vector<vk::Image>& GetSwapchainImages() const {
    return mSwapchainImages;
  }
  vk::SurfaceFormatKHR& GetSurfaceFormat() { return mSurfaceFormat; }
  vk::Extent2D& GetSwapchainExtent() { return mSwapchainExtent; }
  vk::SwapchainCreateInfoKHR& GetSwapchainCreateInfo() { return mSwapchainCreateInfo; }
  [[nodiscard]] const Window& GetWindow() const { return mWindow; }

  // Raw Vulkan Getters
  // TODO: Minimize these raw getters. Instead, provide higher-level functions that perform actions
  // on these resources.
  [[nodiscard]] vk::RenderPass GetRenderPass() const { return *mRenderPass; }
  [[nodiscard]] vk::CommandPool GetCommandPool() const { return *mCommandPool; }
  [[nodiscard]] const std::vector<vk::UniqueImageView>& GetSwapchainImageViews() const {
    return mSwapchainImageViews;
  }
  [[nodiscard]] const std::vector<vk::UniqueFramebuffer>& GetFramebuffers() const {
    return mFramebuffers;
  }

  vk::ResultValue<uint32_t> GetVulkanImages(EngineEventManager& engineEventManager,
                                            vk::UniqueSemaphore& imageAcquired);
  void PresentImage(EngineEventManager& engineEventManager, uint32_t imageIndex);
  void RebuildPerImageSync();

private:
  void SetupQueues();
  vk::UniqueInstance CreateVulkanApplicationInstance();
  void SetupDeviceExtensions();
  void CreateVulkanDevice();
  void CreateCommandPool();
  void CreateSwapchain();
  void CreateRenderPass();
  void CreateSwapchainDependentResources();
  void CreateFramebuffers();

  vk::UniqueInstance mInstance;
  std::vector<const char*> mDeviceExts;
  // TODO: Abstract physical device selection to allow choosing the best device based on
  // features/performance
  vk::PhysicalDevice mPhysDev;
  vk::PhysicalDeviceFeatures2 mFeatures2;
  vk::PhysicalDeviceVulkan12Features mFeatures12;
  vk::PhysicalDeviceVulkan13Features mFeatures13;
  const GlfwContext& mGlfwCtx;
  const Window& mWindow;
  int mMaxFramesInFlight;
  uint32_t mGraphicsFamilyIndex{};
  vk::Queue mGraphicsQueue;
  vk::UniqueSurfaceKHR mSurface;
  vk::UniqueDevice mVulkanDevice;
  vk::UniqueCommandPool mCommandPool;
  vk::UniqueSwapchainKHR mSwapchain;
  std::vector<vk::Image> mSwapchainImages;
  vk::SurfaceFormatKHR mSurfaceFormat;
  vk::Extent2D mSwapchainExtent;
  vk::SurfaceCapabilitiesKHR mSurfaceCapabilities;
  vk::SwapchainCreateInfoKHR mSwapchainCreateInfo;

  // Manual Swapchain Resources
  vk::UniqueRenderPass mRenderPass;
  std::vector<vk::UniqueImageView> mSwapchainImageViews;
  std::vector<vk::UniqueFramebuffer> mFramebuffers;

  std::vector<FrameSync> mFrameSyncs;
  std::vector<SwapchainSync> mSwapchainSyncs;
};

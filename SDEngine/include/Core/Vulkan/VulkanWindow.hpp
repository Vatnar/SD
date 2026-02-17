#pragma once
#include "Core/Events/window/WindowEvents.hpp"
#include "Core/LayerList.hpp"
#include "Core/Vulkan/VulkanConfig.hpp"
#include "Core/Window.hpp"
#include "VulkanContext.hpp"

namespace SD {
// TODO: Consider creating a "FrameData" struct to hold all per-frame resources (semaphores, fences,
// command buffers, etc.)
struct FrameSync {
  vk::UniqueSemaphore imageAcquired;
  vk::UniqueFence inFlight;
};

struct SwapchainSync {
  vk::UniqueSemaphore renderComplete;
};

class VulkanWindow {
public:
  uint32_t CurrentFrame = 0;
  uint32_t CurrentImageIndex = 0;
  explicit VulkanWindow(Window& mWindow, VulkanContext& vulkanCtx);
  ~VulkanWindow();
  void RecreateSwapchain(LayerList& layers);
  [[nodiscard]] const Window& GetWindow() const;


  // TODO: a lot of htese we prolly dont need
  FrameSync& GetFrameSync();
  SwapchainSync& GetSwapchainSync(uint32_t imageIndex);

  vk::UniqueSwapchainKHR& GetSwapchain();
  vk::SwapchainCreateInfoKHR& GetSwapchainCreateInfo();
  [[nodiscard]] const std::vector<vk::Image>& GetSwapchainImages() const;
  vk::SurfaceFormatKHR& GetSurfaceFormat();
  vk::Extent2D& GetSwapchainExtent();
  [[nodiscard]] const std::vector<vk::UniqueImageView>& GetSwapchainImageViews() const;

  [[nodiscard]] const std::vector<vk::UniqueFramebuffer>& GetFramebuffers() const;

  [[nodiscard]] vk::RenderPass GetRenderPass() const;

  vk::ResultValue<uint32_t> GetVulkanImages(vk::UniqueSemaphore& imageAcquired);
  vk::Result PresentImage(uint32_t imageIndex);

  void RebuildPerImageSync();

  [[nodiscard]] vk::CommandPool GetCommandPool() const;

  void Resize(int witdh, int height);

  bool IsMinimized() const { return mIsMinimized; }
  bool IsFramebufferResized() const { return mFramebufferResized; }
  void ResetFramebufferResized() { mFramebufferResized = false; }

  vk::CommandBuffer GetCurrentCommandBuffer() const { return *mCommandBuffers[CurrentFrame]; }

private:
  void CreateSwapchain();
  void CreateRenderPass(); // can be shared, but yeah...

  void CreateCommandPool();

  void CreateSwapchainDependentResources();
  void CreateFramebuffers();

  VulkanContext& mVulkanCtx;
  vk::Device& mDevice;


  Window& mWindow;

  vk::UniqueSurfaceKHR mSurface;

  vk::UniqueSwapchainKHR mSwapchain;
  vk::SwapchainCreateInfoKHR mSwapchainCreateInfo;

  std::vector<vk::Image> mSwapchainImages;
  std::vector<vk::UniqueImageView> mSwapchainImageViews;
  vk::Extent2D mSwapchainExtent;
  vk::SurfaceFormatKHR mSurfaceFormat;
  vk::SurfaceCapabilitiesKHR mSurfaceCapabilities;

  vk::UniqueRenderPass mRenderPass;

  vk::UniqueCommandPool mCommandPool;
  std::vector<vk::UniqueCommandBuffer> mCommandBuffers;

  std::vector<vk::UniqueFramebuffer> mFramebuffers;

  std::vector<FrameSync> mFrameSyncs;
  std::vector<SwapchainSync> mSwapchainSyncs;

  bool mIsMinimized{};
  bool mFramebufferResized{};
};
} // namespace SD

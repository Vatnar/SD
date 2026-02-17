#pragma once
#include "VulkanConfig.hpp"
#include "VulkanContext.hpp"
#include "VulkanWindow.hpp"

namespace SD {
class VulkanRenderer {
public:
  explicit VulkanRenderer(VulkanContext& ctx);

  vk::CommandBuffer BeginFrame(VulkanWindow& vw); // retrieve swapchain
  vk::Result EndFrame(VulkanWindow& vw);          // retrieve swapchain

private:
  VulkanContext& ctx;
};
} // namespace SD

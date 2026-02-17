#pragma once
#include "VulkanConfig.hpp"
#include "VulkanContext.hpp"
#include "VulkanWindow.hpp"

namespace SD {
class VulkanRenderer {
public:
  explicit VulkanRenderer(VulkanContext& ctx);

  vk::CommandBuffer BeginCommandBuffer(VulkanWindow& vw);
  void BeginRenderPass(VulkanWindow& vw);
  vk::CommandBuffer BeginFrame(VulkanWindow& vw); // Legacy, calls both
  vk::Result EndFrame(VulkanWindow& vw);

  void SetClearColor(const std::array<float, 4>& color) { mClearColor = color; }

private:
  VulkanContext& ctx;
  std::array<float, 4> mClearColor{0.1f, 0.1f, 0.1f, 1.0f};
};
} // namespace SD

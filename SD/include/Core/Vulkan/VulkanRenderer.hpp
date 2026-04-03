// TODO(docs): Add file-level Doxygen header
//   - @file VulkanRenderer.hpp
//   - @brief Frame rendering orchestration
//   - Relationship to VulkanContext, VulkanWindow, PipelineFactory, ShaderLibrary
#pragma once

#include "Core/Base.hpp"
#include "Core/Vulkan/PipelineFactory.hpp"
#include "Core/Vulkan/ShaderLibrary.hpp"
#include "VulkanConfig.hpp"
#include "VulkanContext.hpp"
#include "VulkanWindow.hpp"

namespace SD {
// TODO(docs): Document VulkanRenderer class
//   - Purpose: Orchestrates frame rendering (BeginFrame, BeginRenderPass, EndFrame)
//   - Command buffer management
//   - Clear color and render state
//   - Subsystems (ShaderLibrary, PipelineFactory)
//   - Example rendering loop
class VulkanRenderer {
public:
  explicit VulkanRenderer(VulkanContext& ctx);

  vk::CommandBuffer BeginCommandBuffer(VulkanWindow& vw);
  void BeginRenderPass(VulkanWindow& vw);
  vk::CommandBuffer BeginFrame(VulkanWindow& vw); // Legacy, calls both
  vk::Result EndFrame(VulkanWindow& vw);

  void SetClearColor(const std::array<float, 4>& color) { mClearColor = color; }

  // Subsystems
  ShaderLibrary& GetShaderLibrary() { return *mShaders; }
  PipelineFactory& GetPipelineFactory() { return *mPipelines; }

private:
  VulkanContext& ctx;
  VkDevice mDevice;

  std::array<float, 4> mClearColor{0.1f, 0.1f, 0.1f, 1.0f};

  std::unique_ptr<ShaderLibrary> mShaders;
  std::unique_ptr<PipelineFactory> mPipelines;
};
} // namespace SD


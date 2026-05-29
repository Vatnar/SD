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

namespace sd {
// TODO(docs): Document VulkanRenderer class
//   - Purpose: Orchestrates frame rendering (BeginFrame, BeginRenderPass, EndFrame)
//   - Command buffer management
//   - Clear color and render state
//   - Subsystems (ShaderLibrary, PipelineFactory)
//   - Example rendering loop
class VulkanRenderer {
public:
  explicit VulkanRenderer(VulkanContext& ctx);

  vk::CommandBuffer begin_command_buffer(VulkanWindow& vw);
  void begin_render_pass(VulkanWindow& vw);
  vk::CommandBuffer begin_frame(VulkanWindow& vw); // Legacy, calls both
  vk::Result end_frame(VulkanWindow& vw);

  void set_clear_color(const std::array<float, 4>& color) { m_clear_color = color; }

  // Subsystems
  ShaderLibrary& get_shader_library() { return *m_shaders; }
  PipelineFactory& get_pipeline_factory() { return *m_pipelines; }

private:
  VulkanContext& ctx;
  VkDevice m_device;

  std::array<float, 4> m_clear_color{0.1f, 0.1f, 0.1f, 1.0f};

  std::unique_ptr<ShaderLibrary> m_shaders;
  std::unique_ptr<PipelineFactory> m_pipelines;
};
} // namespace SD


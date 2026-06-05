// TODO(docs): Add file-level Doxygen header
//   - @file VulkanRenderer.hpp
//   - @brief Frame rendering orchestration
//   - Relationship to VulkanContext, VulkanWindow, ShaderLibrary
#pragma once

#include "SD/core/FrameTimer.hpp"
#include "SD/core/base.hpp"
#include "SD/core/vulkan/ShaderLibrary.hpp"
#include "VulkanContext.hpp"
#include "VulkanWindow.hpp"
#include "vulkan_config.hpp"

namespace sd {
// TODO(docs): Document VulkanRenderer class
//   - Purpose: Orchestrates frame rendering (BeginFrame, BeginRenderPass, EndFrame)
//   - Command buffer management
//   - Clear color and render state
//   - Subsystem: ShaderLibrary
//   - Example rendering loop
class VulkanRenderer {
public:
  VulkanRenderer(VulkanContext& ctx, FrameTimer& timer);
  void init();

  vk::CommandBuffer begin_command_buffer(VulkanWindow& vw);
  void              begin_render_pass(VulkanWindow& vw);
  vk::CommandBuffer begin_frame(VulkanWindow& vw); // Legacy, calls both
  vk::Result        end_frame(VulkanWindow& vw);

  void set_clear_color(const std::array<float, 4>& color) { m_clear_color = color; }

  void reload_shaders();

  //~ Subsystems
  ShaderLibrary& get_shader_library() { return *m_shaders; }

private:
  VulkanContext& ctx;
  VkDevice       m_device;
  FrameTimer&    m_timer;

  std::array<float, 4> m_clear_color{0.1f, 0.1f, 0.1f, 1.0f};

  std::unique_ptr<ShaderLibrary> m_shaders;
};
} // namespace sd

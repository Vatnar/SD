#pragma once

#include "SD/core/FrameTimer.hpp"
#include "SD/core/base.hpp"
#include "VulkanContext.hpp"
#include "VulkanWindow.hpp"
#include "vulkan_config.hpp"

namespace sd {
struct VulkanRenderer {
  VulkanRenderer(VulkanContext& ctx, FrameTimer& timer);
  void init();

  vk::CommandBuffer begin_command_buffer(VulkanWindow& vw);
  void              begin_render_pass(VulkanWindow& vw);
  vk::CommandBuffer begin_frame(VulkanWindow& vw); // Legacy, calls both
  vk::Result        end_frame(VulkanWindow& vw);

  void set_clear_color(const std::array<float, 4>& color) { m_clear_color = color; }


  VulkanContext& ctx;
  VkDevice       m_device;
  FrameTimer&    m_timer;

  std::array<float, 4> m_clear_color{0.1f, 0.1f, 0.1f, 1.0f};
};
} // namespace sd

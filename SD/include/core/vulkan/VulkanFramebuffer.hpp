#pragma once

#include "core/vulkan/VulkanContext.hpp"
#include "core/vulkan/vulkan_config.hpp"

namespace sd {

class VulkanFramebuffer {
public:
  VulkanFramebuffer(VulkanContext& ctx, uint32_t width, uint32_t height);
  ~VulkanFramebuffer();

  void resize(uint32_t width, uint32_t height);

  vk::Framebuffer get_framebuffer() const { return *m_framebuffer; }
  vk::RenderPass  get_render_pass() const { return *m_render_pass; }
  vk::ImageView   get_color_image_view() const { return *m_color_image_view; }
  vk::Extent2D    get_extent() const { return m_extent; }

private:
  void create_resources();
  void destroy_resources();

private:
  VulkanContext& m_ctx;
  vk::Extent2D   m_extent;

  vk::UniqueImage        m_color_image;
  vk::UniqueDeviceMemory m_color_image_memory;
  vk::UniqueImageView    m_color_image_view;

  vk::UniqueRenderPass  m_render_pass;
  vk::UniqueFramebuffer m_framebuffer;
};

} // namespace sd

#include "Core/Vulkan/VulkanFramebuffer.hpp"

#include "Utils/Utils.hpp"

namespace sd {

VulkanFramebuffer::VulkanFramebuffer(VulkanContext& ctx, u32 width, u32 height) :
  m_ctx(ctx), m_extent(width, height) {
  create_resources();
}

VulkanFramebuffer::~VulkanFramebuffer() {
  destroy_resources();
}

void VulkanFramebuffer::resize(u32 width, u32 height) {
  if (m_extent.width == width && m_extent.height == height)
    return;

  m_extent.width = width;
  m_extent.height = height;

  (void)m_ctx.get_vulkan_device()->waitIdle();
  destroy_resources();
  create_resources();
}

void VulkanFramebuffer::create_resources() {
  auto& device = m_ctx.get_vulkan_device();
  auto& phys_dev = m_ctx.get_physical_device();

  // 1. Create Color Image
  auto [image, memory] =
      create_image(*device, phys_dev, m_extent.width, m_extent.height, vk::Format::eR8G8B8A8Srgb,
                  vk::ImageTiling::eOptimal,
                  vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled,
                  vk::MemoryPropertyFlagBits::eDeviceLocal);
  m_color_image = std::move(image);
  m_color_image_memory = std::move(memory);

  // 2. Create Image View
  vk::ImageViewCreateInfo view_info({}, *m_color_image, vk::ImageViewType::e2D,
                                   vk::Format::eR8G8B8A8Srgb, {},
                                   {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1});
  m_color_image_view = check_vulkan_res_val(device->createImageViewUnique(view_info),
                                      "Failed to create color image view");

  // 3. Create Render Pass
  vk::AttachmentDescription color_attachment{};
  color_attachment.setFormat(vk::Format::eR8G8B8A8Srgb)
      .setSamples(vk::SampleCountFlagBits::e1)
      .setLoadOp(vk::AttachmentLoadOp::eClear)
      .setStoreOp(vk::AttachmentStoreOp::eStore)
      .setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
      .setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
      .setInitialLayout(vk::ImageLayout::eUndefined)
      .setFinalLayout(vk::ImageLayout::eShaderReadOnlyOptimal);

  vk::AttachmentReference color_attachment_ref(0, vk::ImageLayout::eColorAttachmentOptimal);

  vk::SubpassDescription subpass{};
  subpass.setPipelineBindPoint(vk::PipelineBindPoint::eGraphics)
      .setColorAttachmentCount(1)
      .setPColorAttachments(&color_attachment_ref);

  vk::SubpassDependency dependency{};
  dependency.setSrcSubpass(VK_SUBPASS_EXTERNAL)
      .setDstSubpass(0)
      .setSrcStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput)
      .setSrcAccessMask(vk::AccessFlagBits::eNone)
      .setDstStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput)
      .setDstAccessMask(vk::AccessFlagBits::eColorAttachmentWrite);

  vk::RenderPassCreateInfo render_pass_info({}, 1, &color_attachment, 1, &subpass, 1, &dependency);
  m_render_pass = check_vulkan_res_val(device->createRenderPassUnique(render_pass_info),
                                  "Failed to create offscreen render pass");

  // 4. Create Framebuffer
  vk::ImageView attachments[] = {*m_color_image_view};
  vk::FramebufferCreateInfo framebuffer_info({}, *m_render_pass, 1, attachments, m_extent.width,
                                            m_extent.height, 1);
  m_framebuffer = check_vulkan_res_val(device->createFramebufferUnique(framebuffer_info),
                                   "Failed to create offscreen framebuffer");
}

void VulkanFramebuffer::destroy_resources() {
  m_framebuffer.reset();
  m_render_pass.reset();
  m_color_image_view.reset();
  m_color_image.reset();
  m_color_image_memory.reset();
}

} // namespace SD

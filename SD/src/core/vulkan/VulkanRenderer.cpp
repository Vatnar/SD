#include "SD/core/vulkan/VulkanRenderer.hpp"

#include <GLFW/glfw3.h>

#include "SD/core/logging.hpp"
#include "SD/core/vulkan/vulkan_utils.hpp"

namespace sd {
VulkanRenderer::VulkanRenderer(VulkanContext& ctx, FrameTimer& timer) :
  ctx{ctx}, m_device(nullptr), m_timer(timer) {
}

void VulkanRenderer::init() {
  ASSERT(ctx.is_initialized() && "VulkanContext must be initialized before VulkanRenderer::init()");
  m_device = ctx.get_vulkan_device().get();
}

vk::CommandBuffer VulkanRenderer::begin_command_buffer(VulkanWindow& vw) {
  ASSERT(ctx.get_vulkan_device() && "Vulkan device must be valid");
  ASSERT(vw.get_swapchain() && "Swapchain must be valid");
  ASSERT(vw.get_current_command_buffer() && "Command buffer must be allocated");
  ASSERT(vw.current_frame < g_max_frames_in_flight && "Frame index out of bounds");

  auto& device     = ctx.get_vulkan_device();
  auto& frame_sync = vw.get_frame_sync();

  ASSERT(frame_sync.in_flight && "In-flight fence must be valid");

  // Wait for previous frame to finish on GPU (track wait time)
  double waitStart = glfwGetTime();
  check_vulkan_res(device->waitForFences(1, &*frame_sync.in_flight, true, UINT64_MAX),
                   "Failed to wait for fences");
  float waitDuration = static_cast<float>(glfwGetTime() - waitStart);
  m_timer.add_gpu_wait_time(waitDuration);

  auto cmd = vw.get_current_command_buffer();
  auto res = cmd.reset();
  ASSERT(res == vk::Result::eSuccess);

  vk::CommandBufferBeginInfo begin_info{.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit};
  res = cmd.begin(begin_info);
  ASSERT(res == vk::Result::eSuccess);

  return cmd;
}

void VulkanRenderer::begin_render_pass(VulkanWindow& vw) {
  ASSERT(vw.get_swapchain() && "Swapchain must be valid");
  ASSERT(vw.current_image_index < vw.get_swapchain_image_views().size() &&
         "Image index out of bounds");

  auto cmd    = vw.get_current_command_buffer();
  auto extent = vw.get_swapchain_extent();

  // Transition swapchain image: UNDEFINED -> COLOR_ATTACHMENT_OPTIMAL
  auto                   swapchain_image = vw.get_swapchain_images()[vw.current_image_index];
  vk::ImageMemoryBarrier to_att{
      .srcAccessMask       = vk::AccessFlagBits::eNone,
      .dstAccessMask       = vk::AccessFlagBits::eColorAttachmentWrite,
      .oldLayout           = vk::ImageLayout::eUndefined,
      .newLayout           = vk::ImageLayout::eColorAttachmentOptimal,
      .srcQueueFamilyIndex = vk::QueueFamilyIgnored,
      .dstQueueFamilyIndex = vk::QueueFamilyIgnored,
      .image               = swapchain_image,
      .subresourceRange = vk::ImageSubresourceRange{.aspectMask   = vk::ImageAspectFlagBits::eColor,
                                                    .baseMipLevel = 0,
                                                    .levelCount   = 1,
                                                    .baseArrayLayer = 0,
                                                    .layerCount     = 1},
  };
  cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe,
                      vk::PipelineStageFlagBits::eColorAttachmentOutput,
                      {},
                      {},
                      {},
                      to_att);

  vk::RenderingAttachmentInfo color_att{
      .imageView   = *vw.get_swapchain_image_views()[vw.current_image_index],
      .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
      .loadOp      = vk::AttachmentLoadOp::eClear,
      .storeOp     = vk::AttachmentStoreOp::eStore,
      .clearValue  = vk::ClearValue{.color = vk::ClearColorValue{m_clear_color}},
  };
  vk::RenderingInfo rendering_info{
      .renderArea           = {.offset = {0, 0}, .extent = extent},
      .layerCount           = 1,
      .colorAttachmentCount = 1,
      .pColorAttachments    = &color_att,
  };

  cmd.beginRendering(rendering_info);

  vk::Viewport viewport(0.0f,
                        0.0f,
                        static_cast<float>(extent.width),
                        static_cast<float>(extent.height),
                        0.0f,
                        1.0f);
  cmd.setViewport(0, 1, &viewport);

  vk::Rect2D scissor({0, 0}, extent);
  cmd.setScissor(0, 1, &scissor);
}

vk::CommandBuffer VulkanRenderer::begin_frame(VulkanWindow& vw) {
  auto cmd = begin_command_buffer(vw);
  begin_render_pass(vw);
  return cmd;
}

vk::Result VulkanRenderer::end_frame(VulkanWindow& vw) {
  ASSERT(vw.get_swapchain() && "Swapchain must be valid");
  ASSERT(vw.current_image_index < vw.get_swapchain_images().size() && "Image index out of bounds");

  auto cmd = vw.get_current_command_buffer();

  cmd.endRendering();

  // Transition swapchain image: COLOR_ATTACHMENT_OPTIMAL -> PRESENT_SRC_KHR
  auto                   swapchain_image = vw.get_swapchain_images()[vw.current_image_index];
  vk::ImageMemoryBarrier to_present{
      .srcAccessMask       = vk::AccessFlagBits::eColorAttachmentWrite,
      .dstAccessMask       = vk::AccessFlagBits::eNone,
      .oldLayout           = vk::ImageLayout::eColorAttachmentOptimal,
      .newLayout           = vk::ImageLayout::ePresentSrcKHR,
      .srcQueueFamilyIndex = vk::QueueFamilyIgnored,
      .dstQueueFamilyIndex = vk::QueueFamilyIgnored,
      .image               = swapchain_image,
      .subresourceRange = vk::ImageSubresourceRange{.aspectMask   = vk::ImageAspectFlagBits::eColor,
                                                    .baseMipLevel = 0,
                                                    .levelCount   = 1,
                                                    .baseArrayLayer = 0,
                                                    .layerCount     = 1},
  };
  cmd.pipelineBarrier(vk::PipelineStageFlagBits::eColorAttachmentOutput,
                      vk::PipelineStageFlagBits::eBottomOfPipe,
                      {},
                      {},
                      {},
                      to_present);

  (void)cmd.end();

  vk::SubmitInfo submit_info{};

  vk::Semaphore          wait_semaphores[] = {*vw.get_frame_sync().image_acquired};
  vk::PipelineStageFlags wait_stages[]     = {vk::PipelineStageFlagBits::eColorAttachmentOutput};
  submit_info.setWaitSemaphoreCount(1);
  submit_info.setPWaitSemaphores(wait_semaphores);
  submit_info.setPWaitDstStageMask(wait_stages);

  submit_info.setCommandBufferCount(1);
  submit_info.setPCommandBuffers(&cmd);

  vk::Semaphore signal_semaphores[] = {
      *vw.get_swapchain_sync(vw.current_image_index).render_complete};
  submit_info.setSignalSemaphoreCount(1);
  submit_info.setPSignalSemaphores(signal_semaphores);

  auto& device = ctx.get_vulkan_device();
  ASSERT(device && "Device must be valid");
  (void)device->resetFences(*vw.get_frame_sync().in_flight);

  auto err = ctx.get_graphics_queue().submit(1, &submit_info, *vw.get_frame_sync().in_flight);
  if (err != vk::Result::eSuccess) {
    log::engine::error("Failed to submit draw command buffer!");
    return err;
  }

  vk::Result res = vw.present_image(vw.current_image_index);

  vw.current_frame = (vw.current_frame + 1) % g_max_frames_in_flight;

  return res;
}

} // namespace sd

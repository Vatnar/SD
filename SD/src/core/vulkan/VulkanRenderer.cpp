#include "core/vulkan/VulkanRenderer.hpp"

#include <GLFW/glfw3.h>

#include "core/logging.hpp"

namespace sd {
VulkanRenderer::VulkanRenderer(VulkanContext& ctx, FrameTimer& timer) : ctx{ctx}, m_timer(timer) {
}

void VulkanRenderer::init() {
  ASSERT(ctx.is_initialized() && "VulkanContext must be initialized before VulkanRenderer::init()");
  m_device    = ctx.get_vulkan_device().get();
  m_shaders   = std::make_unique<ShaderLibrary>(m_device);
  m_pipelines = std::make_unique<PipelineFactory>(m_device, *m_shaders);
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

  vk::CommandBufferBeginInfo begin_info(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
  res = cmd.begin(begin_info);
  ASSERT(res == vk::Result::eSuccess);

  return cmd;
}

void VulkanRenderer::begin_render_pass(VulkanWindow& vw) {
  ASSERT(vw.get_swapchain() && "Swapchain must be valid");
  ASSERT(vw.get_render_pass() && "Render pass must be valid");
  ASSERT(!vw.get_framebuffers().empty() && "Framebuffers must not be empty");
  ASSERT(vw.current_image_index < vw.get_framebuffers().size() && "Image index out of bounds");

  auto cmd = vw.get_current_command_buffer();

  std::array<vk::ClearValue, 1> clear_values{};
  clear_values[0].color = vk::ClearColorValue{m_clear_color};

  vk::RenderPassBeginInfo render_pass_begin(vw.get_render_pass(),
                                            *vw.get_framebuffers()[vw.current_image_index],
                                            {
                                                {0, 0},
                                                vw.get_swapchain_extent()
  },
                                            clear_values);

  cmd.beginRenderPass(render_pass_begin, vk::SubpassContents::eInline);

  vk::Viewport viewport(0.0f, 0.0f, static_cast<float>(vw.get_swapchain_extent().width),
                        static_cast<float>(vw.get_swapchain_extent().height), 0.0f, 1.0f);
  cmd.setViewport(0, 1, &viewport);

  vk::Rect2D scissor({0, 0}, vw.get_swapchain_extent());
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

  cmd.endRenderPass();
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

void VulkanRenderer::reload_shaders() {
  (void)ctx.get_vulkan_device()->waitIdle();
  m_shaders->ClearCache();
  auto failures = m_pipelines->recreate_all_pipelines();
  if (!failures.empty()) {
    log::engine::warn("{} pipeline(s) failed to recreate during hot reload", failures.size());
  }
}

} // namespace sd

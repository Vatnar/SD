#include "Core/Vulkan/VulkanRenderer.hpp"

#include <vector>

namespace SD {
VulkanRenderer::VulkanRenderer(VulkanContext& ctx) : ctx{ctx} {
}

vk::CommandBuffer VulkanRenderer::BeginFrame(VulkanWindow& vw) {
  auto& device = ctx.GetVulkanDevice();
  auto& frameSync = vw.GetFrameSync();

  // Wait for previous frame to finish on GPU
  CheckVulkanRes(device->waitForFences(1, &*frameSync.inFlight, true, UINT64_MAX),
                 "Failed to wait for fences");

  auto cmd = vw.GetCurrentCommandBuffer();

  // 1. Begin Command Buffer
  // TODO: Check if we need to reset explicitly if the pool was created with ResetCommandBuffer
  // flag, which implies individual buffer reset.
  cmd.reset();
  vk::CommandBufferBeginInfo beginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
  cmd.begin(beginInfo);

  // 2. Begin Render Pass
  std::array<vk::ClearValue, 1> clearValues{};
  clearValues[0].color = vk::ClearColorValue{
      std::array<float, 4>{0.1f, 0.1f, 0.1f, 1.0f}
  };

  vk::RenderPassBeginInfo renderPassInfo(vw.GetRenderPass(),
                                         *vw.GetFramebuffers()[vw.CurrentImageIndex],
                                         {
                                             {0, 0},
                                             vw.GetSwapchainExtent()
  },
                                         clearValues);

  cmd.beginRenderPass(renderPassInfo, vk::SubpassContents::eInline);

  // Set default viewport and scissor
  vk::Viewport viewport(0.0f, 0.0f, static_cast<float>(vw.GetSwapchainExtent().width),
                        static_cast<float>(vw.GetSwapchainExtent().height), 0.0f, 1.0f);
  cmd.setViewport(0, 1, &viewport);

  vk::Rect2D scissor({0, 0}, vw.GetSwapchainExtent());
  cmd.setScissor(0, 1, &scissor);

  return cmd;
}

vk::Result VulkanRenderer::EndFrame(VulkanWindow& vw) {
  auto cmd = vw.GetCurrentCommandBuffer();

  // 1. End Render Pass
  cmd.endRenderPass();

  // 2. End Command Buffer
  cmd.end();

  // 3. Submit
  vk::SubmitInfo submitInfo{};

  vk::Semaphore waitSemaphores[] = {*vw.GetFrameSync().imageAcquired};
  vk::PipelineStageFlags waitStages[] = {vk::PipelineStageFlagBits::eColorAttachmentOutput};
  submitInfo.setWaitSemaphoreCount(1);
  submitInfo.setPWaitSemaphores(waitSemaphores);
  submitInfo.setPWaitDstStageMask(waitStages);

  submitInfo.setCommandBufferCount(1);
  submitInfo.setPCommandBuffers(&cmd);

  vk::Semaphore signalSemaphores[] = {*vw.GetSwapchainSync(vw.CurrentImageIndex).renderComplete};
  submitInfo.setSignalSemaphoreCount(1);
  submitInfo.setPSignalSemaphores(signalSemaphores);

  // Reset fence before submission
  auto& device = ctx.GetVulkanDevice();
  device->resetFences(*vw.GetFrameSync().inFlight);


  // Submit
  auto err = ctx.GetGraphicsQueue().submit(1, &submitInfo, *vw.GetFrameSync().inFlight);
  if (err != vk::Result::eSuccess) {
    throw std::runtime_error("Failed to submit draw command buffer!");
  }

  // 4. Present
  vk::Result res = vw.PresentImage(vw.CurrentImageIndex);

  // Increase frame index for next frame synchronization
  vw.CurrentFrame = (vw.CurrentFrame + 1) % MAX_FRAMES_IN_FLIGHT;

  return res;
}

} // namespace SD

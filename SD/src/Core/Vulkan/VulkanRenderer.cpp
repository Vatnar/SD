#include "Core/Vulkan/VulkanRenderer.hpp"

#include "Application.hpp"
#include <GLFW/glfw3.h>

namespace SD {
VulkanRenderer::VulkanRenderer(VulkanContext& ctx) : ctx{ctx} {
  mDevice = ctx.GetVulkanDevice().get();
  mShaders = std::make_unique<ShaderLibrary>(mDevice);
  mPipelines = std::make_unique<PipelineFactory>(mDevice, *mShaders);
}

vk::CommandBuffer VulkanRenderer::BeginCommandBuffer(VulkanWindow& vw) {
  auto& device = ctx.GetVulkanDevice();
  auto& frameSync = vw.GetFrameSync();

  // Wait for previous frame to finish on GPU (track wait time)
  double waitStart = glfwGetTime();
  CheckVulkanRes(device->waitForFences(1, &*frameSync.inFlight, true, UINT64_MAX),
                 "Failed to wait for fences");
  float waitDuration = static_cast<float>(glfwGetTime() - waitStart);
  Application::Get().AddGpuWaitTime(waitDuration);

  auto cmd = vw.GetCurrentCommandBuffer();
  cmd.reset();
  vk::CommandBufferBeginInfo beginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
  (void)cmd.begin(beginInfo);

  return cmd;
}

void VulkanRenderer::BeginRenderPass(VulkanWindow& vw) {
  auto cmd = vw.GetCurrentCommandBuffer();

  std::array<vk::ClearValue, 1> clearValues{};
  clearValues[0].color = vk::ClearColorValue{mClearColor};

  vk::RenderPassBeginInfo renderPassInfo(vw.GetRenderPass(),
                                         *vw.GetFramebuffers()[vw.CurrentImageIndex],
                                         {
                                             {0, 0},
                                             vw.GetSwapchainExtent()
  },
                                         clearValues);

  cmd.beginRenderPass(renderPassInfo, vk::SubpassContents::eInline);

  vk::Viewport viewport(0.0f, 0.0f, static_cast<float>(vw.GetSwapchainExtent().width),
                        static_cast<float>(vw.GetSwapchainExtent().height), 0.0f, 1.0f);
  cmd.setViewport(0, 1, &viewport);

  vk::Rect2D scissor({0, 0}, vw.GetSwapchainExtent());
  cmd.setScissor(0, 1, &scissor);
}

vk::CommandBuffer VulkanRenderer::BeginFrame(VulkanWindow& vw) {
  auto cmd = BeginCommandBuffer(vw);
  BeginRenderPass(vw);
  return cmd;
}

vk::Result VulkanRenderer::EndFrame(VulkanWindow& vw) {
  auto cmd = vw.GetCurrentCommandBuffer();

  cmd.endRenderPass();
  (void)cmd.end();

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

  auto& device = ctx.GetVulkanDevice();
  (void)device->resetFences(*vw.GetFrameSync().inFlight);

  auto err = ctx.GetGraphicsQueue().submit(1, &submitInfo, *vw.GetFrameSync().inFlight);
  if (err != vk::Result::eSuccess) {
    throw std::runtime_error("Failed to submit draw command buffer!");
  }

  vk::Result res = vw.PresentImage(vw.CurrentImageIndex);

  vw.CurrentFrame = (vw.CurrentFrame + 1) % MAX_FRAMES_IN_FLIGHT;

  return res;
}

} // namespace SD

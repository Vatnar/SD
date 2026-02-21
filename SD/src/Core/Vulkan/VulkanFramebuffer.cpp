#include "Core/Vulkan/VulkanFramebuffer.hpp"

#include "Utils/Utils.hpp"

namespace SD {

VulkanFramebuffer::VulkanFramebuffer(VulkanContext& ctx, u32 width, u32 height) :
  mCtx(ctx), mExtent(width, height) {
  CreateResources();
}

VulkanFramebuffer::~VulkanFramebuffer() {
  DestroyResources();
}

void VulkanFramebuffer::Resize(u32 width, u32 height) {
  if (mExtent.width == width && mExtent.height == height)
    return;

  mExtent.width = width;
  mExtent.height = height;

  (void)mCtx.GetVulkanDevice()->waitIdle();
  DestroyResources();
  CreateResources();
}

void VulkanFramebuffer::CreateResources() {
  auto& device = mCtx.GetVulkanDevice();
  auto& physDev = mCtx.GetPhysicalDevice();

  // 1. Create Color Image
  auto [image, memory] =
      CreateImage(*device, physDev, mExtent.width, mExtent.height, vk::Format::eR8G8B8A8Srgb,
                  vk::ImageTiling::eOptimal,
                  vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled,
                  vk::MemoryPropertyFlagBits::eDeviceLocal);
  mColorImage = std::move(image);
  mColorImageMemory = std::move(memory);

  // 2. Create Image View
  vk::ImageViewCreateInfo viewInfo({}, *mColorImage, vk::ImageViewType::e2D,
                                   vk::Format::eR8G8B8A8Srgb, {},
                                   {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1});
  mColorImageView = CheckVulkanResVal(device->createImageViewUnique(viewInfo),
                                      "Failed to create color image view");

  // 3. Create Render Pass
  vk::AttachmentDescription colorAttachment{};
  colorAttachment.setFormat(vk::Format::eR8G8B8A8Srgb)
      .setSamples(vk::SampleCountFlagBits::e1)
      .setLoadOp(vk::AttachmentLoadOp::eClear)
      .setStoreOp(vk::AttachmentStoreOp::eStore)
      .setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
      .setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
      .setInitialLayout(vk::ImageLayout::eUndefined)
      .setFinalLayout(vk::ImageLayout::eShaderReadOnlyOptimal);

  vk::AttachmentReference colorAttachmentRef(0, vk::ImageLayout::eColorAttachmentOptimal);

  vk::SubpassDescription subpass{};
  subpass.setPipelineBindPoint(vk::PipelineBindPoint::eGraphics)
      .setColorAttachmentCount(1)
      .setPColorAttachments(&colorAttachmentRef);

  vk::SubpassDependency dependency{};
  dependency.setSrcSubpass(VK_SUBPASS_EXTERNAL)
      .setDstSubpass(0)
      .setSrcStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput)
      .setSrcAccessMask(vk::AccessFlagBits::eNone)
      .setDstStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput)
      .setDstAccessMask(vk::AccessFlagBits::eColorAttachmentWrite);

  vk::RenderPassCreateInfo renderPassInfo({}, 1, &colorAttachment, 1, &subpass, 1, &dependency);
  mRenderPass = CheckVulkanResVal(device->createRenderPassUnique(renderPassInfo),
                                  "Failed to create offscreen render pass");

  // 4. Create Framebuffer
  vk::ImageView attachments[] = {*mColorImageView};
  vk::FramebufferCreateInfo framebufferInfo({}, *mRenderPass, 1, attachments, mExtent.width,
                                            mExtent.height, 1);
  mFramebuffer = CheckVulkanResVal(device->createFramebufferUnique(framebufferInfo),
                                   "Failed to create offscreen framebuffer");
}

void VulkanFramebuffer::DestroyResources() {
  mFramebuffer.reset();
  mRenderPass.reset();
  mColorImageView.reset();
  mColorImage.reset();
  mColorImageMemory.reset();
}

} // namespace SD

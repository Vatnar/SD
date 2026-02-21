#include "Core/View.hpp"

#include "Application.hpp"
VkExtent2D SD::View::GetImGuiExtent() {
  const ImVec2 size = ImGui::GetContentRegionAvail();
  return {static_cast<u32>(size.x), static_cast<u32>(size.y)};
}
VkFormat SD::View::FindDepthFormat() {
  std::vector<VkFormat> depthFormats = {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT,
                                        VK_FORMAT_D24_UNORM_S8_UINT};

  for (const auto& format : depthFormats) {
    VkFormatProperties props;
    auto physicalDevice = Application::Get().GetVulkanContext().GetPhysicalDevice();
    vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &props);

    if (props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
      return format;
    }
  }

  Abort("Failed to find supported depth format");
}

void SD::View::OnGuiRender() {
  if (!mOpen)
    return;
  if (ImGui::Begin(mName.c_str(), &mOpen)) {
    VkExtent2D currentExtent = GetImGuiExtent();
    if (currentExtent.width != mExtent.width || currentExtent.height != mExtent.height) {
      Resize(currentExtent);
    }
    mContentRegionPos = ImGui::GetCursorScreenPos();
    mContentRegionExtent = ImGui::GetContentRegionAvail();

    mLayers.GuiRender();
    if (mDisplayTexDS != VK_NULL_HANDLE) {
      ImGui::Image((ImTextureID)mDisplayTexDS, ImGui::GetContentRegionAvail());
    }
  }
  ImGui::End();
}

// SD::View::~View() {
//   CleanupLayeredRender();
// }

void SD::View::SetupLayeredRender(u32 maxStages, VkExtent2D initialExtent) {
  CleanupLayeredRender();

  mExtent = initialExtent;

  // This ensures projection is set up correctly for the initial extent
  float aspect = (float)mExtent.width / (float)mExtent.height;
  AspectMode effectiveMode = mAspectMode;
  if (mAspectMode == AspectMode::BestFit) {
    effectiveMode = (aspect > 1.0f) ? AspectMode::FixedHeight : AspectMode::FixedWidth;
  }

  if (effectiveMode == AspectMode::FixedHeight) {
    mCameraViewProjection = VLA::Matrix4x4f::Ortho(-aspect, aspect, -1.0f, 1.0f, -1.0f, 1.0f);
  } else {
    float invAspect = 1.0f / aspect;
    mCameraViewProjection = VLA::Matrix4x4f::Ortho(-1.0f, 1.0f, -invAspect, invAspect, -1.0f, 1.0f);
  }

  CreateVulkanResources();
}

void SD::View::CreateVulkanResources() {
  auto& vulkanContext = SD::Application::Get().GetVulkanContext();
  VkDevice device = vulkanContext.GetVulkanDevice().get();
  VmaAllocator allocator = vulkanContext.GetVmaAllocator();
  auto extent = mExtent;

  // 1. Color image (device-local)
  VkImageCreateInfo colorInfo{
      .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
      .imageType = VK_IMAGE_TYPE_2D,
      .format = VK_FORMAT_R8G8B8A8_UNORM,
      .extent = {extent.width, extent.height, 1},
      .mipLevels = 1,
      .arrayLayers = 1,
      .samples = VK_SAMPLE_COUNT_1_BIT,
      .tiling = VK_IMAGE_TILING_OPTIMAL,
      .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
               VK_IMAGE_USAGE_TRANSFER_DST_BIT,
      .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
      .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
  };

  VmaAllocationCreateInfo colorAllocInfo = {};
  colorAllocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

  VK_CHECK(vmaCreateImage(allocator, &colorInfo, &colorAllocInfo, &mColorImage, &mColorAllocation,
                          nullptr));

  vk::ImageViewCreateInfo colorViewInfo{
      {},
      mColorImage,
      vk::ImageViewType::e2D,
      vk::Format::eR8G8B8A8Unorm,
      {},
      {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1}
  };
  mColorView =
      CheckVulkanResVal(vulkanContext.GetVulkanDevice()->createImageViewUnique(colorViewInfo),
                        "Failed to create color view: ");

  // 2. Depth image
  VkFormat depthFormat = FindDepthFormat();
  VkImageCreateInfo depthInfo = colorInfo;
  depthInfo.format = depthFormat;
  depthInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

  VK_CHECK(vmaCreateImage(allocator, &depthInfo, &colorAllocInfo, &mDepthImage, &mDepthAllocation,
                          nullptr));

  vk::ImageViewCreateInfo depthViewInfo{
      {},
      mDepthImage,
      vk::ImageViewType::e2D,
      (vk::Format)depthFormat,
      {},
      {vk::ImageAspectFlagBits::eDepth, 0, 1, 0, 1}
  };
  mDepthView =
      CheckVulkanResVal(vulkanContext.GetVulkanDevice()->createImageViewUnique(depthViewInfo),
                        "Failed to create depth view: ");

  // 3. RenderPass attachments
  vk::AttachmentDescription colorAttach(
      {}, vk::Format::eR8G8B8A8Unorm, vk::SampleCountFlagBits::e1, vk::AttachmentLoadOp::eClear,
      vk::AttachmentStoreOp::eStore, vk::AttachmentLoadOp::eDontCare,
      vk::AttachmentStoreOp::eDontCare, vk::ImageLayout::eUndefined,
      vk::ImageLayout::eShaderReadOnlyOptimal);

  vk::AttachmentDescription depthAttach(
      {}, (vk::Format)depthFormat, vk::SampleCountFlagBits::e1, vk::AttachmentLoadOp::eClear,
      vk::AttachmentStoreOp::eDontCare, vk::AttachmentLoadOp::eDontCare,
      vk::AttachmentStoreOp::eDontCare, vk::ImageLayout::eUndefined,
      vk::ImageLayout::eDepthStencilAttachmentOptimal);
  vk::AttachmentDescription attachments[2] = {colorAttach, depthAttach};

  // Subpass references
  vk::AttachmentReference colorRef(0, vk::ImageLayout::eColorAttachmentOptimal);
  vk::AttachmentReference depthRef(1, vk::ImageLayout::eDepthStencilAttachmentOptimal);

  vk::SubpassDescription subpassBg({}, vk::PipelineBindPoint::eGraphics, {}, nullptr, 1, &colorRef,
                                   nullptr, &depthRef);
  vk::SubpassDescription subpassWorld = subpassBg; // Identical for now
  vk::SubpassDescription subpassUi({}, vk::PipelineBindPoint::eGraphics, {}, nullptr, 1, &colorRef,
                                   nullptr, nullptr);
  vk::SubpassDescription subpasses[3] = {subpassBg, subpassWorld, subpassUi};

  // Subpass dependencies (proper progression)
  vk::SubpassDependency dependencies[4] = {
      {VK_SUBPASS_EXTERNAL,
       0, vk::PipelineStageFlagBits::eColorAttachmentOutput,
       vk::PipelineStageFlagBits::eColorAttachmentOutput,
       {},
       vk::AccessFlagBits::eColorAttachmentWrite,
       vk::DependencyFlagBits::eByRegion                                           },
      {                  0, 1, vk::PipelineStageFlagBits::eColorAttachmentOutput,
       vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::AccessFlagBits::eColorAttachmentWrite,
       vk::AccessFlagBits::eColorAttachmentWrite, vk::DependencyFlagBits::eByRegion},
      {                  1, 2, vk::PipelineStageFlagBits::eColorAttachmentOutput,
       vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::AccessFlagBits::eColorAttachmentWrite,
       vk::AccessFlagBits::eColorAttachmentWrite, vk::DependencyFlagBits::eByRegion},
      {                  2,
       VK_SUBPASS_EXTERNAL, vk::PipelineStageFlagBits::eColorAttachmentOutput,
       vk::PipelineStageFlagBits::eBottomOfPipe,
       vk::AccessFlagBits::eColorAttachmentWrite,
       {},
       vk::DependencyFlagBits::eByRegion                                           }
  };

  if (!mLayeredRP) {
    vk::RenderPassCreateInfo rpInfo{
        {},        (u32)std::size(attachments),  attachments, (u32)std::size(subpasses),
        subpasses, (u32)std::size(dependencies), dependencies};
    mLayeredRP = CheckVulkanResVal(vulkanContext.GetVulkanDevice()->createRenderPassUnique(rpInfo),
                                   "Failed to create render pass: ");
  }

  // Framebuffer (now with valid renderPass)
  vk::ImageView views[] = {mColorView.get(), mDepthView.get()};
  vk::FramebufferCreateInfo fbInfo{
      {}, mLayeredRP.get(), (u32)std::size(views), views, extent.width, extent.height, 1};
  mLayeredFramebuffer =
      CheckVulkanResVal(vulkanContext.GetVulkanDevice()->createFramebufferUnique(fbInfo),
                        "Failed to create framebuffer: ");

  auto& imguiCtx = SD::Application::Get().GetImGuiContext();
  mDisplayTexDS = imguiCtx.CreateTextureFromView(mColorView.get());
}

void SD::View::OnRender(vk::CommandBuffer cmd) {
  if (!mLayeredRP)
    return;

  vk::ClearValue clears[2];
  clears[0].color = vk::ClearColorValue(0.1f, 0.1f, 0.1f, 1.0f);
  clears[1].depthStencil = vk::ClearDepthStencilValue(1.0f, 0);

  vk::RenderPassBeginInfo beginInfo{
      mLayeredRP.get(),
      mLayeredFramebuffer.get(),
      {{0, 0}, mExtent},
      2,
      clears
  };

  cmd.beginRenderPass(beginInfo, vk::SubpassContents::eInline);

  // Viewport/scissor once
  vk::Viewport viewport{0, 0, (float)mExtent.width, (float)mExtent.height, 0, 1};
  cmd.setViewport(0, viewport);
  vk::Rect2D scissor{
      {0, 0},
      mExtent
  };
  cmd.setScissor(0, scissor);

  // Subpass 1: Game layer only (for now)
  cmd.nextSubpass(vk::SubpassContents::eInline);
  if (auto* gameLayer = GetLayerByStage(1))
    gameLayer->OnRender(cmd);

  cmd.endRenderPass();
}

void SD::View::CleanupLayeredRender() {
  auto& application = SD::Application::Get();
  auto& vulkanContext = application.GetVulkanContext();
  VmaAllocator allocator = vulkanContext.GetVmaAllocator();

  if (mDisplayTexDS != VK_NULL_HANDLE) {
    application.GetImGuiContext().RemoveTexture(mDisplayTexDS);
    mDisplayTexDS = VK_NULL_HANDLE;
  }

  // Images and allocations
  if (mColorImage != VK_NULL_HANDLE) {
    vmaDestroyImage(allocator, mColorImage, mColorAllocation);
    mColorImage = VK_NULL_HANDLE;
    mColorAllocation = VK_NULL_HANDLE;
  }

  if (mDepthImage != VK_NULL_HANDLE) {
    vmaDestroyImage(allocator, mDepthImage, mDepthAllocation);
    mDepthImage = VK_NULL_HANDLE;
    mDepthAllocation = VK_NULL_HANDLE;
  }

  // vk::Unique handles (Views, RP, FB) clean themselves up
  mLayeredFramebuffer.reset();
  mColorView.reset();
  mDepthView.reset();
}

void SD::View::Resize(VkExtent2D extent) {
  if (extent.width == 0 || extent.height == 0)
    return;
  if (mExtent.width == extent.width && mExtent.height == extent.height)
    return;

  mExtent = extent;
  mExtentChanged = true;

  auto& vulkanContext = SD::Application::Get().GetVulkanContext();
  CheckVulkanRes(vulkanContext.GetVulkanDevice()->waitIdle(),
                 "Failed to wait for device during resize");

  CleanupLayeredRender();
  CreateVulkanResources();

  float aspect = (float)extent.width / (float)extent.height;
  AspectMode effectiveMode = mAspectMode;
  if (mAspectMode == AspectMode::BestFit) {
    effectiveMode = (aspect > 1.0f) ? AspectMode::FixedHeight : AspectMode::FixedWidth;
  }

  if (effectiveMode == AspectMode::FixedHeight) {
    mCameraViewProjection = VLA::Matrix4x4f::Ortho(-aspect, aspect, -1.0f, 1.0f, -1.0f, 1.0f);
  } else {
    float invAspect = 1.0f / aspect;
    mCameraViewProjection = VLA::Matrix4x4f::Ortho(-1.0f, 1.0f, -invAspect, invAspect, -1.0f, 1.0f);
  }
}

SD::Layer* SD::View::GetLayerByStage(u32 stage) {
  if (stage < mLayersByStage.size()) {
    return mLayersByStage[stage].get();
  }
  return nullptr;
}

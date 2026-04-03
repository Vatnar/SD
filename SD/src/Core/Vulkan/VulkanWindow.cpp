#include "Core/Vulkan/VulkanWindow.hpp"

#include "Core/LayerList.hpp"
#include "Core/Window.hpp"
#include "Utils/Utils.hpp"

namespace SD {
VulkanWindow::VulkanWindow(Window& mWindow, VulkanContext& vulkanContext) :
  mVulkanCtx(vulkanContext), mDevice(vulkanContext.GetVulkanDevice().get()), mWindow(mWindow),
  mFrameSyncs(false), mIsMinimized(false) {
  SD_ASSERT(mVulkanCtx.GetVulkanDevice(), "VulkanContext must have valid device");
  SD_ASSERT(mVulkanCtx.GetInstance(), "VulkanContext must have valid instance");

  mSurface = mWindow.CreateWindowSurface(mVulkanCtx.GetInstance(), nullptr);
  SD_ASSERT(mSurface, "Failed to create window surface");
  CreateCommandPool();
  SD_ASSERT(mCommandPool, "Failed to create command pool");
  CreateSwapchain();
  SD_ASSERT(mSwapchain, "Failed to create swapchain");
  CreateRenderPass();
  SD_ASSERT(mRenderPass, "Failed to create render pass");
  CreateSwapchainDependentResources();

  std::generate_n(std::back_inserter(mFrameSyncs), MAX_FRAMES_IN_FLIGHT, [&] {
    FrameSync f;
    f.imageAcquired =
        CheckVulkanResVal(mDevice.createSemaphoreUnique({}), "Failed to create unique semaphore");
    f.inFlight = CheckVulkanResVal(mDevice.createFenceUnique({vk::FenceCreateFlagBits::eSignaled}),
                                   "Failed to create unique fence: ");
    return f;
  });

  std::generate_n(std::back_inserter(mSwapchainSyncs), GetSwapchainImages().size(), [&] {
    SwapchainSync s;
    s.renderComplete =
        CheckVulkanResVal(mDevice.createSemaphoreUnique({}), "Failed to create unique semaphore");
    return s;
  });
}
VulkanWindow::~VulkanWindow() {
  // Use raw call to avoid Vulkan-Hpp assertions during shutdown
  (void)mDevice.waitIdle();

  mFramebuffers.clear();
  mSwapchainImageViews.clear();
  mSwapchain.reset();
  mSurface.reset();
}

void VulkanWindow::RecreateSwapchain(LayerList& layers) {
  SD_ASSERT(mDevice, "Device must be valid");
  u32 oldWidth = mSwapchainExtent.width;
  u32 oldHeight = mSwapchainExtent.height;

  auto [fbWidth, fbHeight] = mWindow.GetFramebufferSize();

  // Handle minimization or zero size
  if (fbWidth == 0 || fbHeight == 0) {
    mIsMinimized = true;
    return;
  }
  mIsMinimized = false;

  if (static_cast<u32>(fbWidth) == oldWidth && static_cast<u32>(fbHeight) == oldHeight) {
    // Already correct size
    return;
  }

  (void)mDevice.waitIdle();

  mFramebuffers.clear();
  mSwapchainImageViews.clear();

  CreateSwapchain();
  CreateSwapchainDependentResources();

  layers.OnSwapchainRecreated();
}
const Window& VulkanWindow::GetWindow() const {
  return mWindow;
}
FrameSync& VulkanWindow::GetFrameSync() {
  SD_ASSERT(CurrentFrame < MAX_FRAMES_IN_FLIGHT, "CurrentFrame index out of bounds");
  SD_ASSERT(CurrentFrame < mFrameSyncs.size(), "CurrentFrame exceeds frame syncs size");
  return mFrameSyncs[CurrentFrame];
}
SwapchainSync& VulkanWindow::GetSwapchainSync(const u32 imageIndex) {
  SD_ASSERT(imageIndex < mSwapchainSyncs.size(), "Image index out of bounds");
  return mSwapchainSyncs[imageIndex];
}
vk::UniqueSwapchainKHR& VulkanWindow::GetSwapchain() {
  return mSwapchain;
}
vk::SwapchainCreateInfoKHR& VulkanWindow::GetSwapchainCreateInfo() {
  return mSwapchainCreateInfo;
}
const std::vector<vk::Image>& VulkanWindow::GetSwapchainImages() const {
  return mSwapchainImages;
}
vk::SurfaceFormatKHR& VulkanWindow::GetSurfaceFormat() {
  return mSurfaceFormat;
}
vk::Extent2D& VulkanWindow::GetSwapchainExtent() {
  return mSwapchainExtent;
}
const std::vector<vk::UniqueImageView>& VulkanWindow::GetSwapchainImageViews() const {
  return mSwapchainImageViews;
}

const std::vector<vk::UniqueFramebuffer>& VulkanWindow::GetFramebuffers() const {
  return mFramebuffers;
}
vk::RenderPass VulkanWindow::GetRenderPass() const {
  return *mRenderPass;
}

std::expected<u32, vk::Result> VulkanWindow::GetVulkanImages(vk::UniqueSemaphore& imageAcquired) {
  SD_ASSERT(mSwapchain, "Swapchain must be valid");
  SD_ASSERT(imageAcquired, "Semaphore must be valid");
  u32 imageIndex;
  vk::Result res =
      mDevice.acquireNextImageKHR(*mSwapchain, UINT64_MAX, *imageAcquired, nullptr, &imageIndex);

  CurrentImageIndex = imageIndex;
  if (res == vk::Result::eSuccess || res == vk::Result::eSuboptimalKHR) {
    return imageIndex;
  }
  return std::unexpected(res);
}

vk::Result VulkanWindow::PresentImage(u32 imageIndex) {
  SD_ASSERT(mSwapchain, "Swapchain must be valid");
  SD_ASSERT(imageIndex < mSwapchainSyncs.size(), "Image index out of bounds");
  SD_ASSERT(mSwapchainSyncs[imageIndex].renderComplete, "Render complete semaphore must be valid");

  vk::PresentInfoKHR presentInfo{};
  presentInfo.setWaitSemaphores(*mSwapchainSyncs[imageIndex].renderComplete);
  presentInfo.setSwapchainCount(1);
  presentInfo.setPSwapchains(&*mSwapchain);
  presentInfo.setPImageIndices(&imageIndex);

  // Use the raw C API to avoid Vulkan-Hpp assertions on exit
  VkPresentInfoKHR presentInfoRaw = presentInfo;
  return static_cast<vk::Result>(
      vkQueuePresentKHR(static_cast<VkQueue>(mVulkanCtx.GetGraphicsQueue()), &presentInfoRaw));
}


void VulkanWindow::RebuildPerImageSync() {
  SD_ASSERT(mDevice, "Device must be valid");
  SD_ASSERT(!mSwapchainImages.empty(), "Swapchain images must not be empty");

  const auto imageCount = GetSwapchainImages().size();
  mSwapchainSyncs.clear();
  mSwapchainSyncs.reserve(imageCount);

  std::ranges::generate_n(std::back_inserter(mSwapchainSyncs), static_cast<int>(imageCount), [&] {
    SwapchainSync sync;
    sync.renderComplete =
        CheckVulkanResVal(mDevice.createSemaphoreUnique({}), "Failed to create unique semaphore: ");
    return sync;
  });
}
void VulkanWindow::Resize(int width, int height) {
  if (width == 0 || height == 0) {
    mIsMinimized = true;
    return;
  }

  mIsMinimized = false;
  mFramebufferResized = true;
}
void VulkanWindow::CreateSwapchain() {
  SD_ASSERT(mDevice, "Device must be valid");
  SD_ASSERT(mSurface, "Surface must be valid");

  auto& physDev = mVulkanCtx.GetPhysicalDevice();
  mSurfaceCapabilities = CheckVulkanResVal(physDev.getSurfaceCapabilitiesKHR(mSurface.get()),
                                           "Failed to get sufrace capabilities");
  auto& caps = mSurfaceCapabilities;
  auto [windowWidth, windowHeight] = mWindow.GetWindowSize();

  if (caps.currentExtent.width != UINT32_MAX) {
    mSwapchainExtent = caps.currentExtent;
  } else {
    mSwapchainExtent.width = std::clamp(static_cast<u32>(windowWidth), caps.minImageExtent.width,
                                        caps.maxImageExtent.width);
    mSwapchainExtent.height = std::clamp(static_cast<u32>(windowHeight), caps.minImageExtent.height,
                                         caps.maxImageExtent.height);
  }

  u32 desiredImageCount = caps.minImageCount + 1;
  if (caps.maxImageCount > 0 && desiredImageCount > caps.maxImageCount)
    desiredImageCount = caps.maxImageCount;

  auto surfaceFormats = CheckVulkanResVal(physDev.getSurfaceFormatsKHR(mSurface.get()),
                                          "Failed to get surface formats: ");
  if (surfaceFormats.size() == 1 && surfaceFormats[0].format == vk::Format::eUndefined) {
    mSurfaceFormat.format = vk::Format::eB8G8R8A8Srgb;
    mSurfaceFormat.colorSpace = vk::ColorSpaceKHR::eSrgbNonlinear;
  } else {
    mSurfaceFormat = surfaceFormats[0];
    for (auto& f : surfaceFormats) {
      if (f.format == vk::Format::eB8G8R8A8Srgb &&
          f.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear) {
        mSurfaceFormat = f;
        break;
      }
    }
  }

  auto presentModes = CheckVulkanResVal(physDev.getSurfacePresentModesKHR(mSurface.get()),
                                        "Failed to get surface present modes: ");
  vk::PresentModeKHR presentMode = vk::PresentModeKHR::eFifo;
  for (auto m : presentModes) {
    if (m == vk::PresentModeKHR::eMailbox) {
      presentMode = m;
      break;
    }
  }

  vk::ImageUsageFlags usage =
      vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferDst;

  vk::SurfaceTransformFlagBitsKHR preTransform =
      (caps.supportedTransforms & vk::SurfaceTransformFlagBitsKHR::eIdentity)
          ? vk::SurfaceTransformFlagBitsKHR::eIdentity
          : caps.currentTransform;

  vk::SwapchainKHR oldSwapchain = mSwapchain.get();

  mSwapchainCreateInfo.setSurface(mSurface.get())
      .setMinImageCount(desiredImageCount)
      .setImageFormat(mSurfaceFormat.format)
      .setImageColorSpace(mSurfaceFormat.colorSpace)
      .setImageExtent(mSwapchainExtent)
      .setImageArrayLayers(1)
      .setImageUsage(usage)
      .setPreTransform(preTransform)
      .setPresentMode(presentMode)
      .setClipped(true)
      .setOldSwapchain(oldSwapchain)
      .setImageSharingMode(vk::SharingMode::eExclusive);

  mSwapchain = CheckVulkanResVal(mDevice.createSwapchainKHRUnique(mSwapchainCreateInfo),
                                 "Failed to create unique swapchain: ");
  mSwapchainImages = CheckVulkanResVal(mDevice.getSwapchainImagesKHR(*mSwapchain),
                                       "Failed to get swapchain images");
  SD_ASSERT(!mSwapchainImages.empty(), "Swapchain must have at least one image");
}
void VulkanWindow::CreateRenderPass() {
  SD_ASSERT(mDevice, "Device must be valid");
  SD_ASSERT(mSurfaceFormat.format != vk::Format::eUndefined, "Surface format must be valid");

  // TODO: Use dynamic rendering (VK_KHR_dynamic_rendering) to simplify render pass management
  vk::AttachmentDescription colorAttachment{};
  colorAttachment.setFormat(mSurfaceFormat.format)
      .setSamples(vk::SampleCountFlagBits::e1)
      .setLoadOp(vk::AttachmentLoadOp::eClear)
      .setStoreOp(vk::AttachmentStoreOp::eStore)
      .setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
      .setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
      .setInitialLayout(vk::ImageLayout::eUndefined)
      .setFinalLayout(vk::ImageLayout::ePresentSrcKHR);

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

  mRenderPass = CheckVulkanResVal(mDevice.createRenderPassUnique(renderPassInfo),
                                  "Failed to create unique renderpass");
}
void VulkanWindow::CreateSwapchainDependentResources() {
  SD_ASSERT(mDevice, "Device must be valid");
  SD_ASSERT(!mSwapchainImages.empty(), "Swapchain images must not be empty");

  mSwapchainImageViews.clear();
  mFramebuffers.clear();

  mSwapchainImageViews.reserve(mSwapchainImages.size());
  for (const auto& image : mSwapchainImages) {
    vk::ImageViewCreateInfo createInfo{};
    createInfo.setImage(image)
        .setViewType(vk::ImageViewType::e2D)
        .setFormat(mSurfaceFormat.format)
        .setComponents({vk::ComponentSwizzle::eIdentity, vk::ComponentSwizzle::eIdentity,
                        vk::ComponentSwizzle::eIdentity, vk::ComponentSwizzle::eIdentity})
        .setSubresourceRange({vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1});

    mSwapchainImageViews.push_back(CheckVulkanResVal(mDevice.createImageViewUnique(createInfo),
                                                     "Failed to create unique imageview: "));
  }

  CreateFramebuffers();
}

void VulkanWindow::CreateFramebuffers() {
  SD_ASSERT(mDevice, "Device must be valid");
  SD_ASSERT(mRenderPass, "Render pass must be valid");
  SD_ASSERT(!mSwapchainImageViews.empty(), "Image views must not be empty");
  SD_ASSERT(mSwapchainExtent.width > 0 && mSwapchainExtent.height > 0,
            "Swapchain extent must be valid");

  mFramebuffers.resize(mSwapchainImageViews.size());

  for (usize i = 0; i < mSwapchainImageViews.size(); i++) {
    vk::ImageView attachments[] = {*mSwapchainImageViews[i]};

    vk::FramebufferCreateInfo framebufferInfo{};
    framebufferInfo.setRenderPass(*mRenderPass)
        .setAttachmentCount(1)
        .setPAttachments(attachments)
        .setWidth(mSwapchainExtent.width)
        .setHeight(mSwapchainExtent.height)
        .setLayers(1);

    mFramebuffers[i] = CheckVulkanResVal(mDevice.createFramebufferUnique(framebufferInfo),
                                         "Failed to create unique framebuffer: ");
  }
}

void VulkanWindow::CreateCommandPool() {
  SD_ASSERT(mDevice, "Device must be valid");

  // 1. Create Pool
  vk::CommandPoolCreateInfo poolInfo(vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
                                     mVulkanCtx.GetGraphicsFamilyIndex());
  mCommandPool = CheckVulkanResVal(mDevice.createCommandPoolUnique(poolInfo),
                                   "Failed to create unique command pool");

  // 2. Allocate Buffers (CRITICAL MISSING STEP)
  vk::CommandBufferAllocateInfo allocInfo{};
  allocInfo.setCommandPool(*mCommandPool)
      .setLevel(vk::CommandBufferLevel::ePrimary)
      .setCommandBufferCount(MAX_FRAMES_IN_FLIGHT);

  mCommandBuffers = CheckVulkanResVal(mDevice.allocateCommandBuffersUnique(allocInfo),
                                      "Failed to allocate command buffers");
  SD_ASSERT(mCommandBuffers.size() == MAX_FRAMES_IN_FLIGHT,
            "Must allocate MAX_FRAMES_IN_FLIGHT command buffers");
}


vk::CommandPool VulkanWindow::GetCommandPool() const {
  return *mCommandPool;
}
} // namespace SD

#include "VulkanContext.hpp"

#include <algorithm>
#include <format>

#include "GlfwContext.hpp"
#include "LayerList.hpp"
#include "spdlog/spdlog.h"


class LayerList;
VulkanContext::VulkanContext(const GlfwContext& glfwCtx, const Window& window,
                             int maxFramesInFlight) :
  mGlfwCtx(glfwCtx), mWindow(window), mMaxFramesInFlight(maxFramesInFlight) {
  static vk::detail::DynamicLoader dl;
  PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr =
      dl.getProcAddress<PFN_vkGetInstanceProcAddr>("vkGetInstanceProcAddr");

  VULKAN_HPP_DEFAULT_DISPATCHER.init(vkGetInstanceProcAddr);

  mInstance = CreateVulkanApplicationInstance();
  mSurface = mWindow.CreateWindowSurface(mInstance, nullptr);
  SetupDeviceExtensions();
  SetupQueues();

  CreateVulkanDevice();
  CreateCommandPool();

  mGraphicsQueue = mVulkanDevice->getQueue(mGraphicsFamilyIndex, 0);

  CreateSwapchain();
  CreateRenderPass();
  CreateSwapchainDependentResources();
  std::generate_n(std::back_inserter(mFrameSyncs), maxFramesInFlight, [&] {
    FrameSync f;
    f.imageAcquired = mVulkanDevice->createSemaphoreUnique({});
    f.inFlight = mVulkanDevice->createFenceUnique({vk::FenceCreateFlagBits::eSignaled});
    return f;
  });

  std::generate_n(std::back_inserter(mSwapchainSyncs), GetSwapchainImages().size(), [&] {
    SwapchainSync s;
    s.renderComplete = mVulkanDevice->createSemaphoreUnique({});
    return s;
  });
}

VulkanContext::~VulkanContext() {
  mVulkanDevice->waitIdle();
  mFramebuffers.clear();
  mSwapchainImageViews.clear();
  mSwapchain.reset();
  mSurface.reset();
}

void VulkanContext::CreateSwapchain() {
  mSurfaceCapabilities = mPhysDev.getSurfaceCapabilitiesKHR(mSurface.get());
  auto& caps = mSurfaceCapabilities;
  auto [windowWidth, windowHeight] = mWindow.GetWindowSize();

  if (caps.currentExtent.width != UINT32_MAX) {
    mSwapchainExtent = caps.currentExtent;
  } else {
    mSwapchainExtent.width = std::clamp(static_cast<uint32_t>(windowWidth),
                                        caps.minImageExtent.width, caps.maxImageExtent.width);
    mSwapchainExtent.height = std::clamp(static_cast<uint32_t>(windowHeight),
                                         caps.minImageExtent.height, caps.maxImageExtent.height);
  }

  uint32_t desiredImageCount = caps.minImageCount + 1;
  if (caps.maxImageCount > 0 && desiredImageCount > caps.maxImageCount)
    desiredImageCount = caps.maxImageCount;

  auto surfaceFormats = mPhysDev.getSurfaceFormatsKHR(mSurface.get());
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

  auto presentModes = mPhysDev.getSurfacePresentModesKHR(mSurface.get());
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

  mSwapchain = mVulkanDevice->createSwapchainKHRUnique(mSwapchainCreateInfo);
  mSwapchainImages = mVulkanDevice->getSwapchainImagesKHR(*mSwapchain);
}

void VulkanContext::CreateRenderPass() {
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

  mRenderPass = mVulkanDevice->createRenderPassUnique(renderPassInfo);
}

void VulkanContext::CreateSwapchainDependentResources() {
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

    mSwapchainImageViews.push_back(mVulkanDevice->createImageViewUnique(createInfo));
  }

  CreateFramebuffers();
}

void VulkanContext::CreateFramebuffers() {
  mFramebuffers.resize(mSwapchainImageViews.size());

  for (size_t i = 0; i < mSwapchainImageViews.size(); i++) {
    vk::ImageView attachments[] = {*mSwapchainImageViews[i]};

    vk::FramebufferCreateInfo framebufferInfo{};
    framebufferInfo.setRenderPass(*mRenderPass)
        .setAttachmentCount(1)
        .setPAttachments(attachments)
        .setWidth(mSwapchainExtent.width)
        .setHeight(mSwapchainExtent.height)
        .setLayers(1);

    mFramebuffers[i] = mVulkanDevice->createFramebufferUnique(framebufferInfo);
  }
}

void VulkanContext::RecreateSwapchain(LayerList& layers) {
  mVulkanDevice->waitIdle();
  mFramebuffers.clear();
  mSwapchainImageViews.clear();
  mSwapchain.reset();

  vk::SwapchainKHR oldSc = mSwapchain.release();

  mSurface.reset();
  mSurface = mWindow.CreateWindowSurface(mInstance, nullptr);


  CreateSwapchain();
  CreateSwapchainDependentResources();

  layers.OnSwapchainRecreated();
}

uint32_t VulkanContext::GetVulkanImages(EngineEventManager& engineEventManager,
                                        vk::UniqueSemaphore& imageAcquired) {
  vk::Result res;
  uint32_t imageIndex{std::numeric_limits<uint32_t>::max()};
  try {
    auto resultValue = mVulkanDevice->acquireNextImageKHR(
        *GetSwapchain(), std::numeric_limits<uint64_t>::max(), *imageAcquired, vk::Fence());
    res = resultValue.result;
    imageIndex = resultValue.value;
  } catch (vk::SystemError& err) {
    res = static_cast<vk::Result>(err.code().value());
  }

  if (res == vk::Result::eErrorOutOfDateKHR || res == vk::Result::eSuboptimalKHR) {
    engineEventManager.PushEvent<SwapchainOutOfDateEvent>();

    imageAcquired = mVulkanDevice->createSemaphoreUnique({});
    // continue;
  } else if (res != vk::Result::eSuccess && res != vk::Result::eSuboptimalKHR) {
    spdlog::get("engine")->critical("Failed to acquire image: {}", vk::to_string(res));
    // continue;
  }
  return imageIndex;
}
void VulkanContext::PresentImage(EngineEventManager& engineEventManager,
                                 const uint32_t imageIndex) {
  auto logger = spdlog::get("engine");

  vk::PresentInfoKHR presentInfo =
      vk::PresentInfoKHR()
          .setWaitSemaphores(*GetSwapchainSync(imageIndex).renderComplete)
          .setSwapchainCount(1)
          .setPSwapchains(&*mSwapchain)
          .setPImageIndices(&imageIndex);

  vk::Result presentResult;
  try {
    presentResult = GetGraphicsQueue().presentKHR(presentInfo);
  } catch (vk::SystemError& err) {
    presentResult = static_cast<vk::Result>(err.code().value());
  }

  if (presentResult == vk::Result::eErrorOutOfDateKHR ||
      presentResult == vk::Result::eSuboptimalKHR)
    engineEventManager.PushEvent<SwapchainOutOfDateEvent>();
  else if (presentResult != vk::Result::eSuccess)
    logger->critical("Failed to present");
}

void VulkanContext::RebuildPerImageSync() {
  const auto imageCount = GetSwapchainImages().size();
  mSwapchainSyncs.clear();
  mSwapchainSyncs.reserve(imageCount);

  std::ranges::generate_n(std::back_inserter(mSwapchainSyncs), static_cast<int>(imageCount), [&] {
    SwapchainSync sync;
    sync.renderComplete = mVulkanDevice->createSemaphoreUnique({});
    return sync;
  });
}

void VulkanContext::SetupQueues() {
  auto queueFamilies = mPhysDev.getQueueFamilyProperties();
  uint32_t graphicsFamilyIndex = UINT32_MAX;

  for (uint32_t i = 0; i < queueFamilies.size(); ++i) {
    if ((queueFamilies[i].queueFlags & vk::QueueFlagBits::eGraphics) ==
        vk::QueueFlagBits::eGraphics) {
      vk::Bool32 supportsPresent = vk::False;
      if (mPhysDev.getSurfaceSupportKHR(i, mSurface.get(), &supportsPresent) ==
              vk::Result::eSuccess &&
          supportsPresent) {
        graphicsFamilyIndex = i;
        break;
      }
    }
  }

  if (graphicsFamilyIndex == UINT32_MAX) {
    Engine::Abort("No queue family supports both graphics and present capabilities");
  }

  mGraphicsFamilyIndex = graphicsFamilyIndex;
}
vk::UniqueInstance VulkanContext::CreateVulkanApplicationInstance() {
  vk::ApplicationInfo appInfo("Engine", 1, "NoEngine", 1, VK_API_VERSION_1_3);

  auto [glfwExts, extCount] = GlfwContext::GetRequiredInstanceExtensions();
  std::vector instanceExts(glfwExts, glfwExts + extCount);

  vk::InstanceCreateInfo instInfo({}, &appInfo, {}, instanceExts);
  vk::UniqueInstance instance = vk::createInstanceUnique(instInfo);

  VULKAN_HPP_DEFAULT_DISPATCHER.init(*instance);

  mPhysDev = instance->enumeratePhysicalDevices().front();
  return instance;
}
void VulkanContext::SetupDeviceExtensions() {
  mFeatures12.timelineSemaphore = VK_TRUE;
  mFeatures12.bufferDeviceAddress = VK_TRUE;
  mFeatures13.setDynamicRendering(true).setSynchronization2(true);
  mFeatures12.pNext = &mFeatures13;
  mFeatures2.setPNext(&mFeatures12);

  auto available = mPhysDev.enumerateDeviceExtensionProperties();
  auto supports = [&](const char* name) {
    return std::ranges::any_of(available, [&](const vk::ExtensionProperties& e) {
      return std::strcmp(e.extensionName, name) == 0;
    });
  };

  auto requireExt = [&](const char* name) {
    if (!supports(name))
      Engine::Abort(std::format("Required device extension missing: {} ", name));
    mDeviceExts.push_back(name);
  };

  requireExt(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
}
void VulkanContext::CreateVulkanDevice() {
  float priority = 1.0f;
  vk::DeviceQueueCreateInfo queueInfo({}, mGraphicsFamilyIndex, 1, &priority);

  vk::DeviceCreateInfo devInfo({}, queueInfo, {}, mDeviceExts);
  devInfo.setPNext(&mFeatures2);

  mVulkanDevice = mPhysDev.createDeviceUnique(devInfo);
  VULKAN_HPP_DEFAULT_DISPATCHER.init(*mInstance, mVulkanDevice.get());
}

void VulkanContext::CreateCommandPool() {
  vk::CommandPoolCreateInfo poolInfo(vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
                                     mGraphicsFamilyIndex);
  mCommandPool = mVulkanDevice->createCommandPoolUnique(poolInfo);
}

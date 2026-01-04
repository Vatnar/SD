#include "VulkanContext.hpp"
#include "GlfwContext.hpp"
#include "spdlog/spdlog.h"


VulkanContext::VulkanContext(const GlfwContext& glfwCtx, const Window& window) : mGlfwCtx(glfwCtx), mWindow(window)
{
    static vk::detail::DynamicLoader dl;
    auto vkGetInstanceProcAddr = dl.getProcAddress<PFN_vkGetInstanceProcAddr>("vkGetInstanceProcAddr");

    VULKAN_HPP_DEFAULT_DISPATCHER.init(vkGetInstanceProcAddr);

    mInstance = CreateVulkanApplicationInstance();
    SetupDeviceExtensions();
    mSurface = window.CreateWindowSurface(mInstance, nullptr);
    SetupQueues();

    CreateVulkanDevice();
}
VulkanContext::~VulkanContext()
{
    mSwapchain.reset();
    mSurface.reset();
}
vk::UniqueSwapchainKHR& VulkanContext::CreateSwapchain()
{
    mSurfaceCapabilities             = mPhysDev.getSurfaceCapabilitiesKHR(mSurface.get());
    auto& caps                       = mSurfaceCapabilities;
    auto [windowWidth, windowHeight] = mWindow.GetWindowSize();

    if (caps.currentExtent.width != UINT32_MAX)
    {
        mSwapchainExtent = caps.currentExtent;
        windowWidth      = static_cast<int>(mSwapchainExtent.width);
        windowHeight     = static_cast<int>(mSwapchainExtent.height);
    }
    else
    {
        // TODO: Change to the actual window width and such, this just for testing
        mSwapchainExtent.width =
                std::clamp(static_cast<uint32_t>(windowWidth), caps.minImageExtent.width, caps.maxImageExtent.width);
        mSwapchainExtent.height =
                std::clamp(static_cast<uint32_t>(windowHeight), caps.minImageExtent.height, caps.maxImageExtent.height);
    }

    // image count
    uint32_t desiredImageCount = caps.minImageCount + 1; // try triple buffering

    if (caps.maxImageCount > 0 && desiredImageCount > caps.maxImageCount)
        desiredImageCount = caps.maxImageCount;


    // NOTE: create swapchain
    auto surfaceFormats = mPhysDev.getSurfaceFormatsKHR(mSurface.get());

    // get surface formats
    if (surfaceFormats.size() == 1 && surfaceFormats[0].format == vk::Format::eUndefined)
    {
        // no preference;
        // todo: study which are best
        mSurfaceFormat.format     = vk::Format::eB8G8R8A8Srgb;
        mSurfaceFormat.colorSpace = vk::ColorSpaceKHR::eSrgbNonlinear;
    }
    else
    {
        // prefer SRGB 8bit BGRA if avilable, else fallback
        mSurfaceFormat = surfaceFormats[0];
        for (auto& f : surfaceFormats)
        {
            if (f.format == vk::Format::eB8G8R8A8Srgb && f.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear)
            {
                mSurfaceFormat = f;
                break;
            }
        }
    }
    // get present mode
    auto               presentModes = mPhysDev.getSurfacePresentModesKHR(mSurface.get());
    vk::PresentModeKHR presentMode  = vk::PresentModeKHR::eFifo; // always supported
    for (auto m : presentModes)
    {
        if (m == vk::PresentModeKHR::eMailbox)
        {
            presentMode = m; // low-latency vsync
            break;
        }
    }

    // find image usage
    vk::ImageUsageFlags usage = vk::ImageUsageFlagBits::eColorAttachment;
    usage |= vk::ImageUsageFlagBits::eTransferDst; // for offscreen hdr buffer blitting

    // find pretransform
    // for rotated displays to work properly
    vk::SurfaceTransformFlagBitsKHR preTransform = caps.supportedTransforms & vk::SurfaceTransformFlagBitsKHR::eIdentity
                                                           ? vk::SurfaceTransformFlagBitsKHR::eIdentity
                                                           : caps.currentTransform;

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
            .setImageSharingMode(vk::SharingMode::eExclusive); // for graphics queue == present queue

    // TODO: errors
    mSwapchain = mVulkanDevice->createSwapchainKHRUnique(mSwapchainCreateInfo);
    return mSwapchain;
}
void VulkanContext::SetupQueues()
{
    auto     queueFamilies       = mPhysDev.getQueueFamilyProperties();
    uint32_t graphicsFamilyIndex = UINT32_MAX;

    for (uint32_t i = 0; i < queueFamilies.size(); ++i)
    {
        bool supportsGraphics =
                (queueFamilies[i].queueFlags & vk::QueueFlagBits::eGraphics) == vk::QueueFlagBits::eGraphics;

        // TODO: ??
        vk::Bool32 supportsPresent = vk::False;

        if (auto res = mPhysDev.getSurfaceSupportKHR(i, mSurface.get(), &supportsPresent); res != vk::Result::eSuccess)
        {
            auto logger = spdlog::get("engine");
            logger->warn("getSurfaceSupportKHR returned vk::Result::{}", static_cast<uint64_t>(res));
        }

        if (supportsGraphics && supportsPresent)
        {
            graphicsFamilyIndex = i;
            break;
        }
    }

    if (graphicsFamilyIndex == UINT32_MAX)
    {
        // TODO: Maybe caller should decide to abort or not
        Engine::Abort("No queue family supports both graphics and present capabilities");
    }

    mGraphicsFamilyIndex = graphicsFamilyIndex;
}
vk::UniqueInstance VulkanContext::CreateVulkanApplicationInstance()
{
    // Create instance
    vk::ApplicationInfo appInfo("Engine", 1, "NoEngine", 1, VK_API_VERSION_1_3);

    auto [glfwExts, extCount] = GlfwContext::GetRequiredInstanceExtensions();

    std::vector instanceExts(glfwExts, glfwExts + extCount);


    vk::InstanceCreateInfo instInfo({}, &appInfo, {}, instanceExts);
    vk::UniqueInstance     instance = vk::createInstanceUnique(instInfo);

    VULKAN_HPP_DEFAULT_DISPATCHER.init(*instance);

    mPhysDev = instance->enumeratePhysicalDevices().front();
    return instance;
}
void VulkanContext::SetupDeviceExtensions()
{
    mFeatures12.timelineSemaphore   = VK_TRUE;
    mFeatures12.bufferDeviceAddress = VK_TRUE;

    mFeatures13.setDynamicRendering(true).setSynchronization2(true);

    mFeatures12.pNext = &mFeatures13;

    mFeatures2.setPNext(&mFeatures12);

    // TODO: actually query for extensions, and not just assume they exist
    auto available = mPhysDev.enumerateDeviceExtensionProperties();

    auto supports = [&](const char *name)
    {
        return std::ranges::any_of(available,
                                   [&](const vk::ExtensionProperties& e)
                                   { return std::strcmp(e.extensionName, name) == 0; });
    };

    auto requireExt = [&](const char *name)
    {
        if (!supports(name))
            Engine::Abort(std::format("Required device extension missing: {} ", name));
        mDeviceExts.push_back(name);
    };

    requireExt(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

    auto raytracing = false;
    if (raytracing)
    {
        requireExt(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME);
        requireExt(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME);
        requireExt(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME);
    }
}
void VulkanContext::CreateVulkanDevice()
{
    float                     priority = 1.0f;
    vk::DeviceQueueCreateInfo queueInfo({}, mGraphicsFamilyIndex, 1, &priority);

    vk::DeviceCreateInfo devInfo({}, queueInfo, {}, mDeviceExts);
    devInfo.setPNext(&mFeatures2);


    mVulkanDevice = mPhysDev.createDeviceUnique(devInfo);
    VULKAN_HPP_DEFAULT_DISPATCHER.init(mVulkanDevice.get());
}

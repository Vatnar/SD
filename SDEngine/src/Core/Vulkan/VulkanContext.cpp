#include "Core/Vulkan/VulkanContext.hpp"

#include <algorithm>

#include "Core/GlfwContext.hpp"
#include "Core/LayerList.hpp"

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE
namespace SD {
class LayerList;

// NOTE: Take in window, since we need it for some initial values, but not to store
VulkanContext::VulkanContext(const GlfwContext& glfwCtx) : mGlfwCtx(glfwCtx) {
  static vk::detail::DynamicLoader dl;
  auto vkGetInstanceProcAddr =
      dl.getProcAddress<PFN_vkGetInstanceProcAddr>("vkGetInstanceProcAddr");

  VULKAN_HPP_DEFAULT_DISPATCHER.init(vkGetInstanceProcAddr);

  mInstance = CreateVulkanApplicationInstance();
  // Temp surface, dies at end of func
}

void VulkanContext::Init(const Window& window) {
  vk::UniqueSurfaceKHR tempSurface = window.CreateWindowSurface(mInstance, nullptr);
  SetupDeviceExtensions();
  SetupQueues(tempSurface.get());

  auto surfaceFormats = CheckVulkanResVal(mPhysDev.getSurfaceFormatsKHR(tempSurface.get()),
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

  CreateVulkanDevice();

  mGraphicsQueue = mVulkanDevice->getQueue(mGraphicsFamilyIndex, 0);
}

VulkanContext::~VulkanContext() {
  // TODO: might want this to teardown Windows
  CheckVulkanRes(mVulkanDevice->waitIdle(), "Failed to wait for vulkan device");
}
vk::UniqueInstance& VulkanContext::GetInstance() {
  return mInstance;
}

std::vector<const char*>& VulkanContext::GetDeviceExtensions() {
  return mDeviceExts;
}

vk::PhysicalDevice& VulkanContext::GetPhysicalDevice() {
  return mPhysDev;
}

vk::UniqueDevice& VulkanContext::GetVulkanDevice() {
  return mVulkanDevice;
}

vk::PhysicalDeviceFeatures2& VulkanContext::GetFeatures2() {
  return mFeatures2;
}

vk::PhysicalDeviceVulkan12Features& VulkanContext::GetFeatures12() {
  return mFeatures12;
}
vk::PhysicalDeviceVulkan13Features& VulkanContext::GetFeatures13() {
  return mFeatures13;
}

uint32_t VulkanContext::GetGraphicsFamilyIndex() const {
  return mGraphicsFamilyIndex;
}
vk::Queue VulkanContext::GetGraphicsQueue() const {
  return mGraphicsQueue;
}

vk::UniqueInstance VulkanContext::CreateVulkanApplicationInstance() {
  vk::ApplicationInfo appInfo("Engine", 1, "NoEngine", 1, VK_API_VERSION_1_3);

  auto [glfwExts, extCount] = GlfwContext::GetRequiredInstanceExtensions();
  std::vector instanceExts(glfwExts, glfwExts + extCount);

  // TODO: Enable validation layers if in debug mode
  vk::InstanceCreateInfo instInfo({}, &appInfo, {}, instanceExts);
  vk::UniqueInstance instance =
      CheckVulkanResVal(vk::createInstanceUnique(instInfo), "Failed to create unique Instance: ");

  VULKAN_HPP_DEFAULT_DISPATCHER.init(*instance);

  mPhysDev = CheckVulkanResVal(instance->enumeratePhysicalDevices(),
                               "Failed to enumerate physical devices: ")
                 .front();
  return instance;
}
void VulkanContext::SetupQueues(vk::SurfaceKHR surface) {
  auto queueFamilies = mPhysDev.getQueueFamilyProperties();
  uint32_t graphicsFamilyIndex = UINT32_MAX;

  for (uint32_t i = 0; i < queueFamilies.size(); ++i) {
    if ((queueFamilies[i].queueFlags & vk::QueueFlagBits::eGraphics) ==
        vk::QueueFlagBits::eGraphics) {
      vk::Bool32 supportsPresent = vk::False;

      // TODO: here's where we need the init window, i think
      // TODO: Create and delete surface
      if (mPhysDev.getSurfaceSupportKHR(i, surface, &supportsPresent) == vk::Result::eSuccess &&
          supportsPresent) {
        graphicsFamilyIndex = i;
        break;
      }
    }
  }

  if (graphicsFamilyIndex == UINT32_MAX) {
    Abort("No queue family supports both graphics and present capabilities");
  }

  mGraphicsFamilyIndex = graphicsFamilyIndex;
}
void VulkanContext::SetupDeviceExtensions() {
  mFeatures12.timelineSemaphore = VK_TRUE;
  mFeatures12.bufferDeviceAddress = VK_TRUE;
  mFeatures13.setDynamicRendering(true).setSynchronization2(true);
  mFeatures12.pNext = &mFeatures13;
  mFeatures2.setPNext(&mFeatures12);

  auto available = CheckVulkanResVal(mPhysDev.enumerateDeviceExtensionProperties(),
                                     "Failed to enumerate device extension properties: ");
  auto supports = [&](const char* name) {
    return std::ranges::any_of(available, [&](const vk::ExtensionProperties& e) {
      return std::strcmp(e.extensionName, name) == 0;
    });
  };

  auto requireExt = [&](const char* name) {
    if (!supports(name))
      Abort("Required device extension missing: " + std::string(name));
    mDeviceExts.push_back(name);
  };

  requireExt(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
}
void VulkanContext::CreateVulkanDevice() {
  float priority = 1.0f;
  vk::DeviceQueueCreateInfo queueInfo({}, mGraphicsFamilyIndex, 1, &priority);

  vk::DeviceCreateInfo devInfo({}, queueInfo, {}, mDeviceExts);
  devInfo.setPNext(&mFeatures2);

  mVulkanDevice =
      CheckVulkanResVal(mPhysDev.createDeviceUnique(devInfo), "Failed to create unique device: ");
  VULKAN_HPP_DEFAULT_DISPATCHER.init(*mInstance, mVulkanDevice.get());
}
} // namespace SD

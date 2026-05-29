#include "Core/Vulkan/VulkanContext.hpp"
#include "Core/Base.hpp"

#include <algorithm>

#include "Core/GlfwContext.hpp"
#include "Core/LayerList.hpp"

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE
namespace sd {
class LayerList;

// NOTE: Take in window, since we need it for some initial values, but not to store
VulkanContext::VulkanContext(const GlfwContext& glfw_ctx) : m_glfw_ctx(glfw_ctx) {
  static vk::detail::DynamicLoader dl;
  m_vk_get_instance_proc_addr =
      dl.getProcAddress<PFN_vkGetInstanceProcAddr>("vkGetInstanceProcAddr");

  VULKAN_HPP_DEFAULT_DISPATCHER.init(m_vk_get_instance_proc_addr);

  m_instance = create_vulkan_application_instance();
  // Temp surface, dies at end of func
}

void VulkanContext::init(const Window& window) {
  vk::UniqueSurfaceKHR temp_surface = window.create_window_surface(m_instance, nullptr);
  setup_device_extensions();
  setup_queues(temp_surface.get());

  auto surface_formats = check_vulkan_res_val(m_phys_dev.getSurfaceFormatsKHR(temp_surface.get()),
                                          "Failed to get surface formats: ");
  if (surface_formats.size() == 1 && surface_formats[0].format == vk::Format::eUndefined) {
    m_surface_format.format = vk::Format::eB8G8R8A8Srgb;
    m_surface_format.colorSpace = vk::ColorSpaceKHR::eSrgbNonlinear;
  } else {
    m_surface_format = surface_formats[0];
    for (auto& f : surface_formats) {
      if (f.format == vk::Format::eB8G8R8A8Srgb &&
          f.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear) {
        m_surface_format = f;
        break;
      }
    }
  }

  create_vulkan_device();

  m_graphics_queue = m_vulkan_device->getQueue(m_graphics_family_index, 0);

  // Initialize VMA
  VmaVulkanFunctions vulkan_functions = {};
  vulkan_functions.vkGetInstanceProcAddr = m_vk_get_instance_proc_addr;
  vulkan_functions.vkGetDeviceProcAddr = m_vk_get_device_proc_addr;

  VmaAllocatorCreateInfo allocator_create_info = {};
  allocator_create_info.vulkanApiVersion = VK_API_VERSION_1_3;
  allocator_create_info.physicalDevice = m_phys_dev;
  allocator_create_info.device = m_vulkan_device.get();
  allocator_create_info.instance = m_instance.get();
  allocator_create_info.pVulkanFunctions = &vulkan_functions;
  allocator_create_info.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;

  if (vmaCreateAllocator(&allocator_create_info, &m_allocator) != VK_SUCCESS) {
    engine_abort("Failed to create VMA allocator");
  }
}

VulkanContext::~VulkanContext() {
  // TODO: might want this to teardown Windows
  if (m_allocator != VK_NULL_HANDLE) {
    vmaDestroyAllocator(m_allocator);
    m_allocator = VK_NULL_HANDLE;
  }
  if (m_vulkan_device) {
    check_vulkan_res(m_vulkan_device->waitIdle(), "Failed to wait for vulkan device");
  }
}
vk::UniqueInstance& VulkanContext::get_instance() {
  return m_instance;
}

std::vector<const char*>& VulkanContext::get_device_extensions() {
  return m_device_exts;
}

vk::PhysicalDevice& VulkanContext::get_physical_device() {
  return m_phys_dev;
}

vk::UniqueDevice& VulkanContext::get_vulkan_device() {
  return m_vulkan_device;
}

vk::PhysicalDeviceFeatures2& VulkanContext::get_features2() {
  return m_features2;
}

vk::PhysicalDeviceVulkan12Features& VulkanContext::get_features12() {
  return m_features12;
}
vk::PhysicalDeviceVulkan13Features& VulkanContext::get_features13() {
  return m_features13;
}

u32 VulkanContext::get_graphics_family_index() const {
  return m_graphics_family_index;
}
vk::Queue VulkanContext::get_graphics_queue() const {
  return m_graphics_queue;
}

vk::UniqueInstance VulkanContext::create_vulkan_application_instance() {
  vk::ApplicationInfo app_info("Engine", 1, "NoEngine", 1, VK_API_VERSION_1_3);

  auto [glfw_exts, ext_count] = GlfwContext::get_required_instance_extensions();
  std::vector instance_exts(glfw_exts, glfw_exts + ext_count);

  // TODO: Enable validation layers if in debug mode
  vk::InstanceCreateInfo inst_info({}, &app_info, {}, instance_exts);
  vk::UniqueInstance instance =
      check_vulkan_res_val(vk::createInstanceUnique(inst_info), "Failed to create unique Instance: ");

  VULKAN_HPP_DEFAULT_DISPATCHER.init(*instance);

  m_phys_dev = check_vulkan_res_val(instance->enumeratePhysicalDevices(),
                               "Failed to enumerate physical devices: ")
                 .front();
  return instance;
}
void VulkanContext::setup_queues(vk::SurfaceKHR surface) {
  auto queue_families = m_phys_dev.getQueueFamilyProperties();
  u32 graphics_family_index = UINT32_MAX;

  for (u32 i = 0; i < queue_families.size(); ++i) {
    if ((queue_families[i].queueFlags & vk::QueueFlagBits::eGraphics) ==
        vk::QueueFlagBits::eGraphics) {
      vk::Bool32 supports_present = vk::False;

      // TODO: here's where we need the init window, i think
      // TODO: Create and delete surface
      if (m_phys_dev.getSurfaceSupportKHR(i, surface, &supports_present) == vk::Result::eSuccess &&
          supports_present) {
        graphics_family_index = i;
        break;
      }
    }
  }

  if (graphics_family_index == UINT32_MAX) {
    engine_abort("No queue family supports both graphics and present capabilities");
  }

  m_graphics_family_index = graphics_family_index;
}
void VulkanContext::setup_device_extensions() {
  // Enable features for wireframe rendering (VK_POLYGON_MODE_LINE)
  m_features2.features.fillModeNonSolid = VK_TRUE;
  
  m_features12.timelineSemaphore = VK_TRUE;
  m_features12.bufferDeviceAddress = VK_TRUE;
  m_features13.setDynamicRendering(true).setSynchronization2(true);
  m_features12.pNext = &m_features13;
  m_features2.setPNext(&m_features12);

  auto available = check_vulkan_res_val(m_phys_dev.enumerateDeviceExtensionProperties(),
                                     "Failed to enumerate device extension properties: ");
  auto supports = [&](const char* name) {
    return std::ranges::any_of(available, [&](const vk::ExtensionProperties& e) {
      return std::strcmp(e.extensionName, name) == 0;
    });
  };

  auto require_ext = [&](const char* name) {
    if (!supports(name))
      engine_abort("Required device extension missing: " + std::string(name));
    m_device_exts.push_back(name);
  };

  require_ext(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
}
void VulkanContext::create_vulkan_device() {
  float priority = 1.0f;
  vk::DeviceQueueCreateInfo queue_info({}, m_graphics_family_index, 1, &priority);

  vk::DeviceCreateInfo dev_info({}, queue_info, {}, m_device_exts);
  dev_info.setPNext(&m_features2);

  m_vulkan_device =
      check_vulkan_res_val(m_phys_dev.createDeviceUnique(dev_info), "Failed to create unique device: ");
  m_vk_get_device_proc_addr = reinterpret_cast<PFN_vkGetDeviceProcAddr>(m_vulkan_device->getProcAddr("vkGetDeviceProcAddr"));
  VULKAN_HPP_DEFAULT_DISPATCHER.init(*m_instance, m_vulkan_device.get());
}
} // namespace SD

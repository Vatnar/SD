#include "SD/core/vulkan/VulkanWindow.hpp"

#include "SD/core/LayerList.hpp"
#include "SD/core/Window.hpp"
#include "SD/utils/utils.hpp"

namespace sd {
VulkanWindow::VulkanWindow(Window& window, VulkanContext& vulkan_context) :
  m_vulkan_ctx(vulkan_context), m_device(vulkan_context.get_vulkan_device().get()),
  m_window(window), m_frame_syncs(false), m_is_minimized(false) {
  ASSERT(m_vulkan_ctx.get_vulkan_device() && "VulkanContext must have valid device");
  ASSERT(m_vulkan_ctx.get_instance() && "VulkanContext must have valid instance");

  m_surface = window.create_window_surface(m_vulkan_ctx.get_instance(), nullptr);
  ASSERT(m_surface && "Failed to create window surface");
  create_command_pool();
  ASSERT(m_command_pool && "Failed to create command pool");
  create_swapchain();
  ASSERT(m_swapchain && "Failed to create swapchain");
  create_render_pass();
  ASSERT(m_render_pass && "Failed to create render pass");
  create_swapchain_dependent_resources();

  std::generate_n(std::back_inserter(m_frame_syncs), g_max_frames_in_flight, [&] {
    FrameSync f;
    f.image_acquired = check_vulkan_res_val(m_device.createSemaphoreUnique({}),
                                            "Failed to create unique semaphore");
    f.in_flight =
        check_vulkan_res_val(m_device.createFenceUnique({vk::FenceCreateFlagBits::eSignaled}),
                             "Failed to create unique fence: ");
    return f;
  });

  std::generate_n(std::back_inserter(m_swapchain_syncs), get_swapchain_images().size(), [&] {
    SwapchainSync s;
    s.render_complete = check_vulkan_res_val(m_device.createSemaphoreUnique({}),
                                             "Failed to create unique semaphore");
    return s;
  });
}
VulkanWindow::~VulkanWindow() {
  // Use raw call to avoid Vulkan-Hpp ASSERTions during shutdown
  (void)m_device.waitIdle();

  m_framebuffers.clear();
  m_swapchain_image_views.clear();
  m_swapchain.reset();
  m_surface.reset();
}

void VulkanWindow::recreate_swapchain(LayerList& layers) {
  ASSERT(m_device && "Device must be valid");
  u32 old_width  = m_swapchain_extent.width;
  u32 old_height = m_swapchain_extent.height;

  auto [fb_width, fb_height] = m_window.get_framebuffer_size();

  // Handle minimization or zero size
  if (fb_width == 0 || fb_height == 0) {
    m_is_minimized = true;
    return;
  }
  m_is_minimized = false;

  if (static_cast<u32>(fb_width) == old_width && static_cast<u32>(fb_height) == old_height) {
    // Already correct size
    return;
  }

  (void)m_device.waitIdle();

  m_framebuffers.clear();
  m_swapchain_image_views.clear();

  create_swapchain();
  create_swapchain_dependent_resources();

  layers.on_swapchain_recreated();
}
const Window& VulkanWindow::get_window() const {
  return m_window;
}
FrameSync& VulkanWindow::get_frame_sync() {
  ASSERT(current_frame < g_max_frames_in_flight && "CurrentFrame index out of bounds");
  ASSERT(current_frame < m_frame_syncs.size() && "CurrentFrame exceeds frame syncs size");
  return m_frame_syncs[current_frame];
}
SwapchainSync& VulkanWindow::get_swapchain_sync(const u32 image_index) {
  ASSERT(image_index < m_swapchain_syncs.size() && "Image index out of bounds");
  return m_swapchain_syncs[image_index];
}
vk::UniqueSwapchainKHR& VulkanWindow::get_swapchain() {
  return m_swapchain;
}
vk::SwapchainCreateInfoKHR& VulkanWindow::get_swapchain_create_info() {
  return m_swapchain_create_info;
}
const std::vector<vk::Image>& VulkanWindow::get_swapchain_images() const {
  return m_swapchain_images;
}
vk::SurfaceFormatKHR& VulkanWindow::get_surface_format() {
  return m_surface_format;
}
vk::Extent2D& VulkanWindow::get_swapchain_extent() {
  return m_swapchain_extent;
}
const std::vector<vk::UniqueImageView>& VulkanWindow::get_swapchain_image_views() const {
  return m_swapchain_image_views;
}

const std::vector<vk::UniqueFramebuffer>& VulkanWindow::get_framebuffers() const {
  return m_framebuffers;
}
vk::RenderPass VulkanWindow::get_render_pass() const {
  return *m_render_pass;
}

std::expected<u32, vk::Result>
VulkanWindow::get_vulkan_images(vk::UniqueSemaphore& image_acquired) {
  ASSERT(m_swapchain && "Swapchain must be valid");
  ASSERT(image_acquired && "Semaphore must be valid");
  u32        image_index;
  vk::Result res = m_device.acquireNextImageKHR(*m_swapchain, UINT64_MAX, *image_acquired, nullptr,
                                                &image_index);

  current_image_index = image_index;
  if (res == vk::Result::eSuccess || res == vk::Result::eSuboptimalKHR) {
    return image_index;
  }
  return std::unexpected(res);
}

vk::Result VulkanWindow::present_image(u32 image_index) {
  ASSERT(m_swapchain && "Swapchain must be valid");
  ASSERT(image_index < m_swapchain_syncs.size() && "Image index out of bounds");
  ASSERT(m_swapchain_syncs[image_index].render_complete &&
         "Render complete semaphore must be valid");

  vk::PresentInfoKHR present_info{};
  present_info.setWaitSemaphores(*m_swapchain_syncs[image_index].render_complete);
  present_info.setSwapchainCount(1);
  present_info.setPSwapchains(&*m_swapchain);
  present_info.setPImageIndices(&image_index);

  // Use the raw C API to avoid Vulkan-Hpp ASSERTions on exit
  VkPresentInfoKHR present_info_raw = present_info;
  return static_cast<vk::Result>(vkQueuePresentKHR(
      static_cast<VkQueue>(m_vulkan_ctx.get_graphics_queue()), &present_info_raw));
}


void VulkanWindow::rebuild_per_image_sync() {
  ASSERT(m_device && "Device must be valid");
  ASSERT(!m_swapchain_images.empty() && "Swapchain images must not be empty");

  const auto image_count = get_swapchain_images().size();
  m_swapchain_syncs.clear();
  m_swapchain_syncs.reserve(image_count);

  std::ranges::generate_n(
      std::back_inserter(m_swapchain_syncs), static_cast<int>(image_count), [&] {
        SwapchainSync sync;
        sync.render_complete = check_vulkan_res_val(m_device.createSemaphoreUnique({}),
                                                    "Failed to create unique semaphore: ");
        return sync;
      });
}
void VulkanWindow::resize(int width, int height) {
  if (width == 0 || height == 0) {
    m_is_minimized = true;
    return;
  }

  m_is_minimized           = false;
  m_is_framebuffer_resized = true;
}
void VulkanWindow::create_swapchain() {
  ASSERT(m_device && "Device must be valid");
  ASSERT(m_surface && "Surface must be valid");

  auto& physical_device = m_vulkan_ctx.get_physical_device();
  m_surface_capabilities =
      check_vulkan_res_val(physical_device.getSurfaceCapabilitiesKHR(m_surface.get()),
                           "Failed to get sufrace capabilities");
  auto& caps                         = m_surface_capabilities;
  auto [window_width, window_height] = m_window.get_window_size();

  if (caps.currentExtent.width != UINT32_MAX) {
    m_swapchain_extent = caps.currentExtent;
  } else {
    m_swapchain_extent.width = std::clamp(static_cast<u32>(window_width), caps.minImageExtent.width,
                                          caps.maxImageExtent.width);
    m_swapchain_extent.height = std::clamp(static_cast<u32>(window_height),
                                           caps.minImageExtent.height, caps.maxImageExtent.height);
  }

  u32 desired_image_count = caps.minImageCount + 1;
  if (caps.maxImageCount > 0 && desired_image_count > caps.maxImageCount)
    desired_image_count = caps.maxImageCount;

  auto surface_formats = check_vulkan_res_val(physical_device.getSurfaceFormatsKHR(m_surface.get()),
                                              "Failed to get surface formats: ");
  if (surface_formats.size() == 1 && surface_formats[0].format == vk::Format::eUndefined) {
    m_surface_format.format     = vk::Format::eB8G8R8A8Srgb;
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

  auto present_modes =
      check_vulkan_res_val(physical_device.getSurfacePresentModesKHR(m_surface.get()),
                           "Failed to get surface present modes: ");
  vk::PresentModeKHR present_mode = vk::PresentModeKHR::eFifo;
  for (auto m : present_modes) {
    if (m == vk::PresentModeKHR::eMailbox) {
      present_mode = m;
      break;
    }
  }

  vk::ImageUsageFlags usage =
      vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferDst;

  vk::SurfaceTransformFlagBitsKHR pre_transform =
      caps.supportedTransforms & vk::SurfaceTransformFlagBitsKHR::eIdentity
          ? vk::SurfaceTransformFlagBitsKHR::eIdentity
          : caps.currentTransform;

  vk::SwapchainKHR old_swapchain = m_swapchain.get();

  m_swapchain_create_info.setSurface(m_surface.get())
      .setMinImageCount(desired_image_count)
      .setImageFormat(m_surface_format.format)
      .setImageColorSpace(m_surface_format.colorSpace)
      .setImageExtent(m_swapchain_extent)
      .setImageArrayLayers(1)
      .setImageUsage(usage)
      .setPreTransform(pre_transform)
      .setPresentMode(present_mode)
      .setClipped(true)
      .setOldSwapchain(old_swapchain)
      .setImageSharingMode(vk::SharingMode::eExclusive);

  m_swapchain = check_vulkan_res_val(m_device.createSwapchainKHRUnique(m_swapchain_create_info),
                                     "Failed to create unique swapchain: ");
  m_swapchain_images = check_vulkan_res_val(m_device.getSwapchainImagesKHR(*m_swapchain),
                                            "Failed to get swapchain images");
  ASSERT(!m_swapchain_images.empty() && "Swapchain must have at least one image");
}
void VulkanWindow::create_render_pass() {
  ASSERT(m_device && "Device must be valid");
  ASSERT(m_surface_format.format != vk::Format::eUndefined && "Surface format must be valid");

  // TODO: Use dynamic rendering (VK_KHR_dynamic_rendering) to simplify render pass management
  vk::AttachmentDescription color_attachment{};
  color_attachment.setFormat(m_surface_format.format)
      .setSamples(vk::SampleCountFlagBits::e1)
      .setLoadOp(vk::AttachmentLoadOp::eClear)
      .setStoreOp(vk::AttachmentStoreOp::eStore)
      .setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
      .setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
      .setInitialLayout(vk::ImageLayout::eUndefined)
      .setFinalLayout(vk::ImageLayout::ePresentSrcKHR);

  vk::AttachmentReference color_attachment_ref(0, vk::ImageLayout::eColorAttachmentOptimal);

  vk::SubpassDescription subpass{};
  subpass.setPipelineBindPoint(vk::PipelineBindPoint::eGraphics)
      .setColorAttachmentCount(1)
      .setPColorAttachments(&color_attachment_ref);

  vk::SubpassDependency dependency{};
  dependency.setSrcSubpass(VK_SUBPASS_EXTERNAL)
      .setDstSubpass(0)
      .setSrcStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput)
      .setSrcAccessMask(vk::AccessFlagBits::eNone)
      .setDstStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput)
      .setDstAccessMask(vk::AccessFlagBits::eColorAttachmentWrite);

  vk::RenderPassCreateInfo render_pass_info({}, 1, &color_attachment, 1, &subpass, 1, &dependency);

  m_render_pass = check_vulkan_res_val(m_device.createRenderPassUnique(render_pass_info),
                                       "Failed to create unique renderpass");
}
void VulkanWindow::create_swapchain_dependent_resources() {
  ASSERT(m_device && "Device must be valid");
  ASSERT(!m_swapchain_images.empty() && "Swapchain images must not be empty");

  m_swapchain_image_views.clear();
  m_framebuffers.clear();

  m_swapchain_image_views.reserve(m_swapchain_images.size());
  for (const auto& image : m_swapchain_images) {
    vk::ImageViewCreateInfo create_info{};
    create_info.setImage(image)
        .setViewType(vk::ImageViewType::e2D)
        .setFormat(m_surface_format.format)
        .setComponents({vk::ComponentSwizzle::eIdentity, vk::ComponentSwizzle::eIdentity,
                        vk::ComponentSwizzle::eIdentity, vk::ComponentSwizzle::eIdentity})
        .setSubresourceRange({vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1});

    m_swapchain_image_views.push_back(check_vulkan_res_val(
        m_device.createImageViewUnique(create_info), "Failed to create unique imageview: "));
  }

  create_framebuffers();
}

void VulkanWindow::create_framebuffers() {
  ASSERT(m_device && "Device must be valid");
  ASSERT(m_render_pass && "Render pass must be valid");
  ASSERT(!m_swapchain_image_views.empty() && "Image views must not be empty");
  ASSERT(m_swapchain_extent.width > 0 && m_swapchain_extent.height > 0 &&
         "Swapchain extent must be valid");

  m_framebuffers.resize(m_swapchain_image_views.size());

  for (usize i = 0; i < m_swapchain_image_views.size(); i++) {
    vk::ImageView attachments[] = {*m_swapchain_image_views[i]};

    vk::FramebufferCreateInfo framebuffer_info{};
    framebuffer_info.setRenderPass(*m_render_pass)
        .setAttachmentCount(1)
        .setPAttachments(attachments)
        .setWidth(m_swapchain_extent.width)
        .setHeight(m_swapchain_extent.height)
        .setLayers(1);

    m_framebuffers[i] = check_vulkan_res_val(m_device.createFramebufferUnique(framebuffer_info),
                                             "Failed to create unique framebuffer: ");
  }
}

void VulkanWindow::create_command_pool() {
  ASSERT(m_device && "Device must be valid");

  // 1. Create Pool
  vk::CommandPoolCreateInfo pool_info(vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
                                      m_vulkan_ctx.get_graphics_family_index());
  m_command_pool = check_vulkan_res_val(m_device.createCommandPoolUnique(pool_info),
                                        "Failed to create unique command pool");

  // 2. Allocate Buffers (CRITICAL MISSING STEP)
  vk::CommandBufferAllocateInfo alloc_info{};
  alloc_info.setCommandPool(*m_command_pool)
      .setLevel(vk::CommandBufferLevel::ePrimary)
      .setCommandBufferCount(g_max_frames_in_flight);

  m_command_buffers = check_vulkan_res_val(m_device.allocateCommandBuffersUnique(alloc_info),
                                           "Failed to allocate command buffers");
  ASSERT(m_command_buffers.size() == g_max_frames_in_flight &&
         "Must allocate g_max_frames_in_flight command buffers");
}


vk::CommandPool VulkanWindow::get_command_pool() const {
  return *m_command_pool;
}
} // namespace sd

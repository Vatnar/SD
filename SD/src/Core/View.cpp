#include "Core/View.hpp"

#include <limits>

#include "Application.hpp"
#include "Core/SDImGuiContext.hpp"
#include "Utils/Utils.hpp"

sd::View::~View() {
  // Detach layers first while resources are still valid
  for (auto& layer : m_layers_by_stage) {
    if (layer) {
      layer->on_detach();
      layer.reset();
    }
  }
  m_layers_by_stage.clear();

  cleanup_layered_render();
}

VkExtent2D sd::View::get_im_gui_extent() {
  const ImVec2 size = ImGui::GetContentRegionAvail();

  auto clamp_to_u32 = [](float v) -> u32 {
    if (v <= 0.0f)
      return 1;
    if (v >= static_cast<float>(std::numeric_limits<u32>::max()))
      return std::numeric_limits<u32>::max() - 1;
    return static_cast<u32>(v);
  };

  return {clamp_to_u32(size.x), clamp_to_u32(size.y)};
}
VkFormat sd::View::find_depth_format() {
   std::vector depth_formats = {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT,
                                        VK_FORMAT_D24_UNORM_S8_UINT};

  auto physical_device = Application::get().get_vulkan_context().get_physical_device();
  assert(physical_device&& "Physical device must be valid");

  for (const auto& format : depth_formats) {
    VkFormatProperties props;
    vkGetPhysicalDeviceFormatProperties(physical_device, format, &props);

    if (props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
      return format;
    }
  }

  engine_abort("Failed to find supported depth format");
}

void sd::View::on_gui_render() {
  if (!m_open)
    return;
  if (!ImGui::GetCurrentContext())
    return;
  if (ImGui::Begin(m_name.c_str(), &m_open)) {
    VkExtent2D current_extent = get_im_gui_extent();
    if (current_extent.width != m_extent.width || current_extent.height != m_extent.height) {
      resize(current_extent);
    }
    m_content_region_pos = ImGui::GetCursorScreenPos();
    m_content_region_extent = ImGui::GetContentRegionAvail();

    m_layers.gui_render();
    if (m_display_tex_ds != VK_NULL_HANDLE) {
      ImGui::Image(reinterpret_cast<ImTextureID>(m_display_tex_ds), ImGui::GetContentRegionAvail());
    }
  }
  ImGui::End();
}

// SD::View::~View() {
//   CleanupLayeredRender();
// }

void sd::View::setup_layered_render(u32 maxStages, VkExtent2D initialExtent) {
  assert(initialExtent.width > 0 && initialExtent.height > 0 &&
                   "Initial extent must be valid");
  assert(maxStages > 0 && "Must have at least one stage");

  cleanup_layered_render();

  m_extent = initialExtent;

  // This ensures projection is set up correctly for the initial extent
  float aspect = static_cast<float>(m_extent.width) / static_cast<float>(m_extent.height);
  AspectMode effective_mode = m_aspect_mode;
  if (m_aspect_mode == AspectMode::BEST_FIT) {
    effective_mode = (aspect > 1.0f) ? AspectMode::FIXED_HEIGHT : AspectMode::FIXED_WIDTH;
  }

  if (effective_mode == AspectMode::FIXED_HEIGHT) {
    m_camera_view_projection = VLA::Matrix4x4f::Ortho(-aspect, aspect, -1.0f, 1.0f, -1.0f, 1.0f);
  } else {
    float inv_aspect = 1.0f / aspect;
    m_camera_view_projection = VLA::Matrix4x4f::Ortho(-1.0f, 1.0f, -inv_aspect, inv_aspect, -1.0f, 1.0f);
  }

  create_vulkan_resources();
}

void sd::View::create_vulkan_resources() {
  auto& application = Application::get();
  auto& vulkan_context = application.get_vulkan_context();
  VmaAllocator allocator = vulkan_context.get_vma_allocator();
  auto extent = m_extent;

  assert(vulkan_context.get_vulkan_device()&& "Vulkan device must be valid");
  assert(allocator&& "VMA allocator must be valid");
  assert(extent.width > 0 && extent.height > 0&& "Extent must be valid");

  // Color image (device-local)
  VkImageCreateInfo color_info{
      .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
      .pNext = {},
      .flags = {},
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
      .queueFamilyIndexCount = {},
      .pQueueFamilyIndices = {},
      .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
  };

  VmaAllocationCreateInfo color_alloc_info = {};
  color_alloc_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;

  VK_CHECK(vmaCreateImage(allocator, &color_info, &color_alloc_info, &m_color_image, &m_color_allocation,
                          nullptr));
  assert(m_color_image != VK_NULL_HANDLE&&  "Color image must be created");

  vk::ImageViewCreateInfo color_view_info{
      {},
      m_color_image,
      vk::ImageViewType::e2D,
      vk::Format::eR8G8B8A8Unorm,
      {},
      {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1}
  };
  m_color_view =
      check_vulkan_res_val(vulkan_context.get_vulkan_device()->createImageViewUnique(color_view_info),
                        "Failed to create color view: ");
  assert(m_color_view && "Color view must be created");

  //  Depth image
  VkFormat depth_format = find_depth_format();
  VkImageCreateInfo depth_info = color_info;
  depth_info.format = depth_format;
  depth_info.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

  VK_CHECK(vmaCreateImage(allocator, &depth_info, &color_alloc_info, &m_depth_image, &m_depth_allocation,
                          nullptr));
  assert(m_depth_image != VK_NULL_HANDLE && "Depth image must be created");

  vk::ImageViewCreateInfo depth_view_info{
      {},
      m_depth_image,
      vk::ImageViewType::e2D,
      static_cast<vk::Format>(depth_format),
      {},
      {vk::ImageAspectFlagBits::eDepth, 0, 1, 0, 1}
  };
  m_depth_view =
      check_vulkan_res_val(vulkan_context.get_vulkan_device()->createImageViewUnique(depth_view_info),
                        "Failed to create depth view: ");
  assert(m_depth_view && "Depth view must be created");

  //  RenderPass attachments
  vk::AttachmentDescription color_attach(
      {}, vk::Format::eR8G8B8A8Unorm, vk::SampleCountFlagBits::e1, vk::AttachmentLoadOp::eClear,
      vk::AttachmentStoreOp::eStore, vk::AttachmentLoadOp::eDontCare,
      vk::AttachmentStoreOp::eDontCare, vk::ImageLayout::eUndefined,
      vk::ImageLayout::eShaderReadOnlyOptimal);
  vk::AttachmentDescription depth_attach(
      {}, static_cast<vk::Format>(depth_format), vk::SampleCountFlagBits::e1,
      vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eDontCare,
      vk::AttachmentLoadOp::eDontCare, vk::AttachmentStoreOp::eDontCare,
      vk::ImageLayout::eUndefined, vk::ImageLayout::eDepthStencilAttachmentOptimal);
  vk::AttachmentDescription attachments[2] = {color_attach, depth_attach};

  // Subpass references
  vk::AttachmentReference color_ref(0, vk::ImageLayout::eColorAttachmentOptimal);
  vk::AttachmentReference depth_ref(1, vk::ImageLayout::eDepthStencilAttachmentOptimal);

  vk::SubpassDescription subpass_bg({}, vk::PipelineBindPoint::eGraphics, {}, nullptr, 1, &color_ref,
                                   nullptr, &depth_ref);
  vk::SubpassDescription subpass_world = subpass_bg; // Identical for now
  vk::SubpassDescription subpass_ui({}, vk::PipelineBindPoint::eGraphics, {}, nullptr, 1, &color_ref,
                                   nullptr, nullptr);
  vk::SubpassDescription subpasses[3] = {subpass_bg, subpass_world, subpass_ui};

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

  if (!m_layered_rp) {
    vk::RenderPassCreateInfo rp_info{
        {},        static_cast<u32>(std::size(attachments)),  attachments, static_cast<u32>(std::size(subpasses)),
        subpasses, static_cast<u32>(std::size(dependencies)), dependencies};
    m_layered_rp = check_vulkan_res_val(vulkan_context.get_vulkan_device()->createRenderPassUnique(rp_info),
                                   "Failed to create render pass: ");
    assert(m_layered_rp && "Render pass must be created");
  }

  // Framebuffer (now with valid renderPass)
  vk::ImageView views[] = {m_color_view.get(), m_depth_view.get()};
  vk::FramebufferCreateInfo fb_info{
      {}, m_layered_rp.get(), static_cast<u32>(std::size(views)), views, extent.width, extent.height, 1};
  m_layered_framebuffer =
      check_vulkan_res_val(vulkan_context.get_vulkan_device()->createFramebufferUnique(fb_info),
                        "Failed to create framebuffer: ");
  assert(m_layered_framebuffer && "Framebuffer must be created");

  auto& imguiCtx = application.get_im_gui_context();
  m_display_tex_ds = imguiCtx.create_texture_from_view(m_color_view.get());
}

void sd::View::on_render(vk::CommandBuffer cmd) {
  assert(m_layered_rp && "Render pass must be valid");
  assert(m_layered_framebuffer && "Framebuffer must be valid");
  assert(m_extent.width > 0 && m_extent.height > 0 && "Extent must be valid");

  auto* game_layer = get_layer_by_stage(1);
  if (!game_layer) {
    return;
  }

  vk::ClearValue clears[2];
  clears[0].color = vk::ClearColorValue(0.1f, 0.1f, 0.1f, 1.0f);
  clears[1].depthStencil = vk::ClearDepthStencilValue(1.0f, 0);

  vk::RenderPassBeginInfo begin_info{
      m_layered_rp.get(),
      m_layered_framebuffer.get(),
      {{0, 0}, m_extent},
      2,
      clears
  };

  cmd.beginRenderPass(begin_info, vk::SubpassContents::eInline);

  // Viewport/scissor once
  vk::Viewport viewport{0, 0, static_cast<float>(m_extent.width), static_cast<float>(m_extent.height),
                        0, 1};
  cmd.setViewport(0, viewport);
  vk::Rect2D scissor{
      {0, 0},
      m_extent
  };
  cmd.setScissor(0, scissor);

  // Subpass 1: Game layer only (for now)
  cmd.nextSubpass(vk::SubpassContents::eInline);
  game_layer->on_render(cmd);

  cmd.endRenderPass();
}

void sd::View::cleanup_layered_render() {
  auto& application = Application::get();

  // Check if Vulkan context is still valid
  if (!application.get_vulkan_context().get_vulkan_device()) {
    log::engine::warn("Vulkan context invalid during view cleanup, resetting handles only");
    // Just reset handles, can't properly clean up
    m_color_image = VK_NULL_HANDLE;
    m_color_allocation = VK_NULL_HANDLE;
    m_depth_image = VK_NULL_HANDLE;
    m_depth_allocation = VK_NULL_HANDLE;
    m_display_tex_ds = VK_NULL_HANDLE;
    m_layered_framebuffer.reset();
    m_color_view.reset();
    m_depth_view.reset();
    m_layered_rp.reset();
    return;
  }

  auto& vulkan_context = application.get_vulkan_context();
  VmaAllocator allocator = vulkan_context.get_vma_allocator();

  if (m_display_tex_ds != VK_NULL_HANDLE) {
    application.get_im_gui_context().remove_texture(m_display_tex_ds);
    m_display_tex_ds = VK_NULL_HANDLE;
  }

  // Images and allocations - check allocator validity
  if (m_color_image != VK_NULL_HANDLE) {
    if (allocator) {
      vmaDestroyImage(allocator, m_color_image, m_color_allocation);
    } else {
      log::engine::error("Cannot destroy color image: VMA allocator is null");
    }
    m_color_image = VK_NULL_HANDLE;
    m_color_allocation = VK_NULL_HANDLE;
  }

  if (m_depth_image != VK_NULL_HANDLE) {
    if (allocator) {
      vmaDestroyImage(allocator, m_depth_image, m_depth_allocation);
    } else {
      log::engine::error("Cannot destroy depth image: VMA allocator is null");
    }
    m_depth_image = VK_NULL_HANDLE;
    m_depth_allocation = VK_NULL_HANDLE;
  }

  // vk::Unique handles (Views, RP, FB) clean themselves up
  m_layered_framebuffer.reset();
  m_color_view.reset();
  m_depth_view.reset();
  m_layered_rp.reset();
}

void sd::View::resize(VkExtent2D extent) {
  if (extent.width == 0 || extent.height == 0)
    return;
  if (m_extent.width == extent.width && m_extent.height == extent.height)
    return;

  m_extent = extent;
  m_extent_changed = true;

  auto& vulkan_context = Application::get().get_vulkan_context();
  assert(vulkan_context.get_vulkan_device()&& "Device must be valid");
  check_vulkan_res(vulkan_context.get_vulkan_device()->waitIdle(),
                 "Failed to wait for device during resize");

  cleanup_layered_render();
  create_vulkan_resources();

  float aspect = static_cast<float>(extent.width) / static_cast<float>(extent.height);
  AspectMode effective_mode = m_aspect_mode;
  if (m_aspect_mode == AspectMode::BEST_FIT) {
    effective_mode = (aspect > 1.0f) ? AspectMode::FIXED_HEIGHT : AspectMode::FIXED_WIDTH;
  }

  if (effective_mode == AspectMode::FIXED_HEIGHT) {
    m_camera_view_projection = VLA::Matrix4x4f::Ortho(-aspect, aspect, -1.0f, 1.0f, -1.0f, 1.0f);
  } else {
    float inv_aspect = 1.0f / aspect;
    m_camera_view_projection = VLA::Matrix4x4f::Ortho(-1.0f, 1.0f, -inv_aspect, inv_aspect, -1.0f, 1.0f);
  }
}

sd::Layer* sd::View::get_layer_by_stage(u32 stage) {
  if (stage < m_layers_by_stage.size()) {
    return m_layers_by_stage[stage].get();
  }
  return nullptr;
}

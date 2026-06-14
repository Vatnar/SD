#include "SD/core/View.hpp"

#include <algorithm>
#include <limits>
#include <vector>

#include "SD/Application.hpp"
#include "SD/core/SDImGuiContext.hpp"
#include "SD/utils/utils.hpp"

sd::View::~View() {
  m_layers.clear();
  destroy_viewport();
}

vk::Extent2D sd::View::get_im_gui_extent() {
  const ImVec2 size = ImGui::GetContentRegionAvail();

  auto clamp_to_u32 = [](float v) -> U32 {
    if (v <= 0.0f)
      return 1;
    if (v >= static_cast<float>(std::numeric_limits<U32>::max()))
      return std::numeric_limits<U32>::max() - 1;
    return static_cast<U32>(v);
  };

  return {clamp_to_u32(size.x), clamp_to_u32(size.y)};
}

vk::Format sd::View::find_depth_format() {
  std::array depth_formats = {vk::Format::eD32Sfloat,
                              vk::Format::eD32SfloatS8Uint,
                              vk::Format::eD24UnormS8Uint};

  auto physical_device = m_vulkan_ctx.get_physical_device();
  ASSERT(physical_device && "Physical device must be valid");

  for (const auto& format : depth_formats) {
    vk::FormatProperties props;
    physical_device.getFormatProperties(format, &props);

    if (props.optimalTilingFeatures & vk::FormatFeatureFlagBits::eDepthStencilAttachment) {
      return format;
    }
  }

  log::engine::critical("Failed to find supported depth format");
  return vk::Format::eUndefined;
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
    m_content_region_pos    = ImGui::GetCursorScreenPos();
    m_content_region_extent = ImGui::GetContentRegionAvail();

    m_layers.gui_render();
    if (m_display_tex_ds != VK_NULL_HANDLE) {
      ImGui::Image(reinterpret_cast<ImTextureID>(m_display_tex_ds), ImGui::GetContentRegionAvail());
    }
  }
  ImGui::End();
}

void sd::View::create_viewport(VkExtent2D initialExtent) {
  ASSERT(initialExtent.width > 0 && initialExtent.height > 0 && "Initial extent must be valid");

  destroy_viewport();

  m_extent = initialExtent;

  float      aspect = static_cast<float>(m_extent.width) / static_cast<float>(m_extent.height);
  AspectMode effective_mode = m_aspect_mode;
  if (m_aspect_mode == AspectMode::BEST_FIT) {
    effective_mode = (aspect > 1.0f) ? AspectMode::FIXED_HEIGHT : AspectMode::FIXED_WIDTH;
  }

  if (effective_mode == AspectMode::FIXED_HEIGHT) {
    m_camera_view_projection = VLA::Matrix4x4f::Ortho(-aspect, aspect, -1.0f, 1.0f, -1.0f, 1.0f);
  } else {
    float inv_aspect = 1.0f / aspect;
    m_camera_view_projection =
        VLA::Matrix4x4f::Ortho(-1.0f, 1.0f, -inv_aspect, inv_aspect, -1.0f, 1.0f);
  }

  create_vulkan_resources();
}

void sd::View::create_vulkan_resources() {
  VmaAllocator allocator = m_vulkan_ctx.get_vma_allocator();
  auto         extent    = m_extent;

  ASSERT(m_vulkan_ctx.get_vulkan_device() && "Vulkan device must be valid");
  ASSERT(allocator && "VMA allocator must be valid");
  ASSERT(extent.width > 0 && extent.height > 0 && "Extent must be valid");

  m_color_format = vk::Format::eR8G8B8A8Unorm;
  m_depth_format = find_depth_format();

  // Color image (device-local)
  using enum vk::ImageUsageFlagBits;
  vk::ImageCreateInfo color_info{
      .imageType     = vk::ImageType::e2D,
      .format        = m_color_format,
      .extent        = {extent.width, extent.height, 1},
      .mipLevels     = 1,
      .arrayLayers   = 1,
      .samples       = vk::SampleCountFlagBits::e1,
      .tiling        = vk::ImageTiling::eOptimal,
      .usage         = eColorAttachment | eSampled | eTransferDst,
      .sharingMode   = vk::SharingMode::eExclusive,
      .initialLayout = vk::ImageLayout::eUndefined
  };

  VmaAllocationCreateInfo color_alloc_info = {};
  color_alloc_info.usage                   = VMA_MEMORY_USAGE_GPU_ONLY;


  VK_CHECK(vmaCreateImage(allocator,
                          reinterpret_cast<VkImageCreateInfo*>(&color_info),
                          &color_alloc_info,
                          reinterpret_cast<VkImage*>(&m_color_image),
                          &m_color_allocation,
                          nullptr));


  ASSERT(m_color_image != VK_NULL_HANDLE && "Color image must be created");

  vk::ImageViewCreateInfo color_view_info{
      .image            = m_color_image,
      .viewType         = vk::ImageViewType::e2D,
      .format           = m_color_format,
      .subresourceRange = vk::ImageSubresourceRange{.aspectMask   = vk::ImageAspectFlagBits::eColor,
                                                    .baseMipLevel = 0,
                                                    .levelCount   = 1,
                                                    .baseArrayLayer = 0,
                                                    .layerCount     = 1}
  };
  m_color_view =
      check_vulkan_res_val(m_vulkan_ctx.get_vulkan_device()->createImageViewUnique(color_view_info),
                           "Failed to create color view: ");
  ASSERT(m_color_view && "Color view must be created");

  // Depth image
  vk::ImageCreateInfo depth_info = color_info;
  depth_info.format              = m_depth_format;
  depth_info.usage               = vk::ImageUsageFlagBits::eDepthStencilAttachment;

  VK_CHECK(vmaCreateImage(allocator,
                          reinterpret_cast<VkImageCreateInfo*>(&depth_info),
                          &color_alloc_info,
                          reinterpret_cast<VkImage*>(&m_depth_image),
                          &m_depth_allocation,
                          nullptr));
  ASSERT(m_depth_image != VK_NULL_HANDLE && "Depth image must be created");

  vk::ImageViewCreateInfo depth_view_info{
      .image            = m_depth_image,
      .viewType         = vk::ImageViewType::e2D,
      .format           = m_depth_format,
      .subresourceRange = vk::ImageSubresourceRange{.aspectMask   = vk::ImageAspectFlagBits::eDepth,
                                                    .baseMipLevel = 0,
                                                    .levelCount   = 1,
                                                    .baseArrayLayer = 0,
                                                    .layerCount     = 1}
  };

  m_depth_view =
      check_vulkan_res_val(m_vulkan_ctx.get_vulkan_device()->createImageViewUnique(depth_view_info),
                           "Failed to create depth view: ");
  ASSERT(m_depth_view && "Depth view must be created");

  // Initial transition: UNDEFINED -> SHADER_READ_ONLY_OPTIMAL (for ImGui descriptor)
  {
    auto&                 device = m_vulkan_ctx.get_vulkan_device();
    vk::UniqueCommandPool tmp_pool =
        check_vulkan_res_val(device->createCommandPoolUnique(vk::CommandPoolCreateInfo{
                                 .queueFamilyIndex = m_vulkan_ctx.get_graphics_family_index()}),
                             "Failed to create temp command pool");
    auto tmp_cmds =
        check_vulkan_res_val(device->allocateCommandBuffersUnique(vk::CommandBufferAllocateInfo{
                                 .commandPool        = *tmp_pool,
                                 .level              = vk::CommandBufferLevel::ePrimary,
                                 .commandBufferCount = 1}),
                             "Failed to allocate temp cmd buffer");
    auto tmp_cmd = std::move(tmp_cmds.front());
    (void)tmp_cmd->begin(
        vk::CommandBufferBeginInfo{.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit});
    vk::ImageMemoryBarrier init_barrier{
        .srcAccessMask       = vk::AccessFlagBits::eNone,
        .dstAccessMask       = vk::AccessFlagBits::eShaderRead,
        .oldLayout           = vk::ImageLayout::eUndefined,
        .newLayout           = vk::ImageLayout::eShaderReadOnlyOptimal,
        .srcQueueFamilyIndex = vk::QueueFamilyIgnored,
        .dstQueueFamilyIndex = vk::QueueFamilyIgnored,
        .image               = m_color_image,
        .subresourceRange = vk::ImageSubresourceRange{.aspectMask = vk::ImageAspectFlagBits::eColor,
                                                      .baseMipLevel   = 0,
                                                      .levelCount     = 1,
                                                      .baseArrayLayer = 0,
                                                      .layerCount     = 1},
    };
    tmp_cmd->pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe,
                             vk::PipelineStageFlagBits::eFragmentShader,
                             {},
                             {},
                             {},
                             init_barrier);

    // Initial transition: depth UNDEFINED -> DEPTH_STENCIL_ATTACHMENT_OPTIMAL
    vk::ImageMemoryBarrier depth_init_barrier{
        .srcAccessMask       = vk::AccessFlagBits::eNone,
        .dstAccessMask       = vk::AccessFlagBits::eDepthStencilAttachmentWrite,
        .oldLayout           = vk::ImageLayout::eUndefined,
        .newLayout           = vk::ImageLayout::eDepthStencilAttachmentOptimal,
        .srcQueueFamilyIndex = vk::QueueFamilyIgnored,
        .dstQueueFamilyIndex = vk::QueueFamilyIgnored,
        .image               = m_depth_image,
        .subresourceRange = vk::ImageSubresourceRange{.aspectMask = vk::ImageAspectFlagBits::eDepth,
                                                      .baseMipLevel   = 0,
                                                      .levelCount     = 1,
                                                      .baseArrayLayer = 0,
                                                      .layerCount     = 1},
    };
    tmp_cmd->pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe,
                             vk::PipelineStageFlagBits::eEarlyFragmentTests,
                             {},
                             {},
                             {},
                             depth_init_barrier);

    (void)tmp_cmd->end();
    vk::SubmitInfo submit{.commandBufferCount = 1, .pCommandBuffers = &*tmp_cmd};
    (void)m_vulkan_ctx.get_graphics_queue().submit(submit);
    (void)device->waitIdle();
  }

  m_display_tex_ds = m_imgui_ctx.create_texture_from_view(m_color_view.get());
}

void sd::View::on_render(vk::CommandBuffer cmd) {
  ASSERT(m_color_view && "Color view must be valid");
  ASSERT(m_depth_view && "Depth view must be valid");
  ASSERT(m_extent.width > 0 && m_extent.height > 0 && "Extent must be valid");

  // Collect active layers sorted by stage
  std::vector<Layer*> render_layers;
  for (auto& layer : m_layers) {
    if (layer->is_active()) {
      render_layers.push_back(layer.get());
    }
  }
  if (render_layers.empty()) {
    return;
  }
  std::stable_sort(render_layers.begin(), render_layers.end(), [](const Layer* a, const Layer* b) {
    return a->m_stage_id < b->m_stage_id;
  });

  // Pre-barrier: color SHADER_READ_ONLY -> COLOR_ATTACHMENT_OPTIMAL
  // Depth is already in DEPTH_STENCIL_ATTACHMENT_OPTIMAL (left from last frame)
  vk::ImageMemoryBarrier color_to_att{
      .srcAccessMask       = vk::AccessFlagBits::eNone,
      .dstAccessMask       = vk::AccessFlagBits::eColorAttachmentWrite,
      .oldLayout           = vk::ImageLayout::eShaderReadOnlyOptimal,
      .newLayout           = vk::ImageLayout::eColorAttachmentOptimal,
      .srcQueueFamilyIndex = vk::QueueFamilyIgnored,
      .dstQueueFamilyIndex = vk::QueueFamilyIgnored,
      .image               = m_color_image,
      .subresourceRange = vk::ImageSubresourceRange{.aspectMask   = vk::ImageAspectFlagBits::eColor,
                                                    .baseMipLevel = 0,
                                                    .levelCount   = 1,
                                                    .baseArrayLayer = 0,
                                                    .layerCount     = 1},
  };
  cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe,
                      vk::PipelineStageFlagBits::eColorAttachmentOutput,
                      {},
                      {},
                      {},
                      color_to_att);

  // Each layer is responsible for its own beginRendering/endRendering,
  // viewport/scissor, clears, and load/store ops.
  for (auto* layer : render_layers) {
    layer->on_render(cmd);
  }

  // Post-barrier: color -> SHADER_READ_ONLY_OPTIMAL for ImGui display
  vk::ImageMemoryBarrier color_to_read{
      .srcAccessMask       = vk::AccessFlagBits::eColorAttachmentWrite,
      .dstAccessMask       = vk::AccessFlagBits::eShaderRead,
      .oldLayout           = vk::ImageLayout::eColorAttachmentOptimal,
      .newLayout           = vk::ImageLayout::eShaderReadOnlyOptimal,
      .srcQueueFamilyIndex = vk::QueueFamilyIgnored,
      .dstQueueFamilyIndex = vk::QueueFamilyIgnored,
      .image               = m_color_image,
      .subresourceRange = vk::ImageSubresourceRange{.aspectMask   = vk::ImageAspectFlagBits::eColor,
                                                    .baseMipLevel = 0,
                                                    .levelCount   = 1,
                                                    .baseArrayLayer = 0,
                                                    .layerCount     = 1},
  };
  cmd.pipelineBarrier(vk::PipelineStageFlagBits::eColorAttachmentOutput,
                      vk::PipelineStageFlagBits::eFragmentShader,
                      {},
                      {},
                      {},
                      color_to_read);
}

void sd::View::destroy_viewport() {
  if (!m_vulkan_ctx.get_vulkan_device()) {
    log::engine::warn("Vulkan context invalid during view cleanup, resetting handles only");
    m_color_image      = VK_NULL_HANDLE;
    m_color_allocation = VK_NULL_HANDLE;
    m_depth_image      = VK_NULL_HANDLE;
    m_depth_allocation = VK_NULL_HANDLE;
    m_display_tex_ds   = VK_NULL_HANDLE;
    m_color_view.reset();
    m_depth_view.reset();
    return;
  }

  VmaAllocator allocator = m_vulkan_ctx.get_vma_allocator();

  if (m_display_tex_ds != VK_NULL_HANDLE) {
    m_imgui_ctx.remove_texture(m_display_tex_ds);
    m_display_tex_ds = VK_NULL_HANDLE;
  }

  if (m_color_image != VK_NULL_HANDLE) {
    if (allocator) {
      vmaDestroyImage(allocator, m_color_image, m_color_allocation);
    } else {
      log::engine::error("Cannot destroy color image: VMA allocator is null");
    }
    m_color_image      = VK_NULL_HANDLE;
    m_color_allocation = VK_NULL_HANDLE;
  }

  if (m_depth_image != VK_NULL_HANDLE) {
    if (allocator) {
      vmaDestroyImage(allocator, m_depth_image, m_depth_allocation);
    } else {
      log::engine::error("Cannot destroy depth image: VMA allocator is null");
    }
    m_depth_image      = VK_NULL_HANDLE;
    m_depth_allocation = VK_NULL_HANDLE;
  }

  m_color_view.reset();
  m_depth_view.reset();
}

void sd::View::resize(VkExtent2D extent) {
  if (extent.width == 0 || extent.height == 0)
    return;
  if (m_extent.width == extent.width && m_extent.height == extent.height)
    return;

  m_extent         = extent;
  m_extent_changed = true;

  ASSERT(m_vulkan_ctx.get_vulkan_device() && "Device must be valid");
  check_vulkan_res(m_vulkan_ctx.get_vulkan_device()->waitIdle(),
                   "Failed to wait for device during resize");

  destroy_viewport();
  create_vulkan_resources();

  float      aspect         = static_cast<float>(extent.width) / static_cast<float>(extent.height);
  AspectMode effective_mode = m_aspect_mode;
  if (m_aspect_mode == AspectMode::BEST_FIT) {
    effective_mode = (aspect > 1.0f) ? AspectMode::FIXED_HEIGHT : AspectMode::FIXED_WIDTH;
  }

  if (effective_mode == AspectMode::FIXED_HEIGHT) {
    m_camera_view_projection = VLA::Matrix4x4f::Ortho(-aspect, aspect, -1.0f, 1.0f, -1.0f, 1.0f);
  } else {
    float inv_aspect = 1.0f / aspect;
    m_camera_view_projection =
        VLA::Matrix4x4f::Ortho(-1.0f, 1.0f, -inv_aspect, inv_aspect, -1.0f, 1.0f);
  }
}

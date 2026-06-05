#include "SD/core/View.hpp"

#include <limits>

#include "SD/Application.hpp"
#include "SD/core/SDImGuiContext.hpp"
#include "SD/utils/utils.hpp"

sd::View::~View() {
  // Detach layers first while resources are still valid
  for (auto& layer : m_layers_by_stage) {
    if (layer) {
      layer->on_detach();
      layer.reset();
    }
  }
  m_layers_by_stage.clear();
  m_layers.clear();

  cleanup_layered_render();
}

vk::Extent2D sd::View::get_im_gui_extent() {
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
vk::Format sd::View::find_depth_format() {
  std::array depth_formats = {vk::Format::eD32Sfloat, vk::Format::eD32SfloatS8Uint,
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

void sd::View::setup_layered_render(u32 maxStages, VkExtent2D initialExtent) {
  ASSERT(initialExtent.width > 0 && initialExtent.height > 0 && "Initial extent must be valid");
  ASSERT(maxStages > 0 && "Must have at least one stage");

  cleanup_layered_render();

  m_extent = initialExtent;

  // This ensures projection is set up correctly for the initial extent
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

  // Color image (device-local)
  using enum vk::ImageUsageFlagBits;
  vk::ImageCreateInfo color_info{
      .imageType     = vk::ImageType::e2D,
      .format        = vk::Format::eR8G8B8A8Unorm,
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


  VK_CHECK(vmaCreateImage(allocator, reinterpret_cast<VkImageCreateInfo*>(&color_info),
                          &color_alloc_info, reinterpret_cast<VkImage*>(&m_color_image),
                          &m_color_allocation, nullptr));


  ASSERT(m_color_image != VK_NULL_HANDLE && "Color image must be created");

  vk::ImageViewCreateInfo color_view_info{
      .image            = m_color_image,
      .viewType         = vk::ImageViewType::e2D,
      .format           = vk::Format::eR8G8B8A8Unorm,
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

  //  Depth image
  vk::Format          depth_format = find_depth_format();
  vk::ImageCreateInfo depth_info   = color_info;
  depth_info.format                = depth_format;
  depth_info.usage                 = vk::ImageUsageFlagBits::eDepthStencilAttachment;

  VK_CHECK(vmaCreateImage(allocator, reinterpret_cast<VkImageCreateInfo*>(&depth_info),
                          &color_alloc_info, reinterpret_cast<VkImage*>(&m_depth_image),
                          &m_depth_allocation, nullptr));
  ASSERT(m_depth_image != VK_NULL_HANDLE && "Depth image must be created");

  vk::ImageViewCreateInfo depth_view_info{
      .image            = m_depth_image,
      .viewType         = vk::ImageViewType::e2D,
      .format           = depth_format,
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
                             vk::PipelineStageFlagBits::eFragmentShader, {}, {}, {}, init_barrier);
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

  auto* game_layer = get_layer_by_stage(1);
  if (!game_layer) {
    return;
  }

  std::array clears = {
      vk::ClearValue{.color = vk::ClearColorValue{std::array{0.1f, 0.1f, 0.1f, 1.0f}}},
      vk::ClearValue{.depthStencil = vk::ClearDepthStencilValue{1.0f, 0}},
  };

  // Transition color: UNDEFINED -> COLOR_ATTACHMENT_OPTIMAL
  vk::ImageMemoryBarrier color_to_att{
      .srcAccessMask       = vk::AccessFlagBits::eNone,
      .dstAccessMask       = vk::AccessFlagBits::eColorAttachmentWrite,
      .oldLayout           = vk::ImageLayout::eUndefined,
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
                      vk::PipelineStageFlagBits::eColorAttachmentOutput, {}, {}, {}, color_to_att);

  // Transition depth: UNDEFINED -> DEPTH_STENCIL_ATTACHMENT_OPTIMAL
  vk::ImageMemoryBarrier depth_to_att{
      .srcAccessMask       = vk::AccessFlagBits::eNone,
      .dstAccessMask       = vk::AccessFlagBits::eDepthStencilAttachmentWrite,
      .oldLayout           = vk::ImageLayout::eUndefined,
      .newLayout           = vk::ImageLayout::eDepthStencilAttachmentOptimal,
      .srcQueueFamilyIndex = vk::QueueFamilyIgnored,
      .dstQueueFamilyIndex = vk::QueueFamilyIgnored,
      .image               = m_depth_image,
      .subresourceRange = vk::ImageSubresourceRange{.aspectMask   = vk::ImageAspectFlagBits::eDepth,
                                                    .baseMipLevel = 0,
                                                    .levelCount   = 1,
                                                    .baseArrayLayer = 0,
                                                    .layerCount     = 1},
  };
  cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe,
                      vk::PipelineStageFlagBits::eEarlyFragmentTests, {}, {}, {}, depth_to_att);

  vk::RenderingAttachmentInfo color_att{
      .imageView   = *m_color_view,
      .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
      .loadOp      = vk::AttachmentLoadOp::eClear,
      .storeOp     = vk::AttachmentStoreOp::eStore,
      .clearValue  = clears[0],
  };
  vk::RenderingAttachmentInfo depth_att{
      .imageView   = *m_depth_view,
      .imageLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal,
      .loadOp      = vk::AttachmentLoadOp::eClear,
      .storeOp     = vk::AttachmentStoreOp::eDontCare,
      .clearValue  = clears[1],
  };
  vk::RenderingInfo rendering_info{
      .renderArea           = {.offset = {0, 0}, .extent = m_extent},
      .layerCount           = 1,
      .colorAttachmentCount = 1,
      .pColorAttachments    = &color_att,
      .pDepthAttachment     = &depth_att,
  };

  cmd.beginRendering(rendering_info);

  vk::Viewport viewport{
      0, 0, static_cast<float>(m_extent.width), static_cast<float>(m_extent.height), 0, 1};
  cmd.setViewport(0, viewport);
  vk::Rect2D scissor{
      {0, 0},
      m_extent
  };
  cmd.setScissor(0, scissor);

  game_layer->on_render(cmd);

  cmd.endRendering();

  // Transition color: COLOR_ATTACHMENT_OPTIMAL -> SHADER_READ_ONLY_OPTIMAL
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
                      vk::PipelineStageFlagBits::eFragmentShader, {}, {}, {}, color_to_read);
}

void sd::View::cleanup_layered_render() {
  if (!m_vulkan_ctx.get_vulkan_device()) {
    log::engine::warn("Vulkan context invalid during view cleanup, resetting handles only");
    // Just reset handles, can't properly clean up
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

  // Images and allocations - check allocator validity
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

  // vk::Unique handles clean themselves up
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

  cleanup_layered_render();
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

sd::Layer* sd::View::get_layer_by_stage(u32 stage) {
  if (stage < m_layers_by_stage.size()) {
    return m_layers_by_stage[stage].get();
  }
  return nullptr;
}

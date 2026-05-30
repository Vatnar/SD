// TODO(docs): Add file-level Doxygen header
//   - @file Utils.hpp
//   - @brief Vulkan utility functions and macros
//   - Note: This is a "kitchen sink" file - consider splitting
//   - Categories: Error checking, buffer/image creation, texture loading
#pragma once
#include <expected>
#include <filesystem>
#include <limits>
#include <stb_image.h>

#include "core/base.hpp"

/**
 * @namespace  engine
 * @brief Namespace for Engine intrinsic things
 */
namespace sd {


// TODO(docs): Document VK_CHECK macro
//   - Purpose: Vulkan error checking with abort on failure
//   - When to use vs CheckVulkanRes
//   - Example usage
#define VK_CHECK(x)               \
  do {                            \
    VkResult res = (x);           \
    if (res != VK_SUCCESS)        \
      engine_abort("Vulkan error: " #x); \
  } while (0)

// SD_DEBUG_ASSERT — debug builds only. Elided in release (NDEBUG).
// SD_ALWAYS_ASSERT — always on, even in release.
// Both accept an optional custom message after the condition.
// Example: SD_ALWAYS_ASSERT(ptr != nullptr, "Pointer was null after allocation");
#ifdef NDEBUG
#define SD_DEBUG_ASSERT(x, ...) ((void)0)
#else
#define SD_DEBUG_ASSERT(x, ...)                                                  \
  do {                                                                         \
    if (!(x)) {                                                                \
      sd::log::engine::error("ASSERT {}:{} {}: {}", __FILE__, __LINE__, #x,   \
                             ## __VA_ARGS__);                                   \
      sd::engine_abort();                                                      \
    }                                                                          \
  } while (0)

#define SD_ALWAYS_ASSERT(x, ...)                                                 \
  do {                                                                         \
    if (!(x)) {                                                                \
      sd::log::engine::error("ASSERT {}:{} {}: {}", __FILE__, __LINE__, #x,   \
                             ## __VA_ARGS__);                                   \
      sd::engine_abort();                                                      \
    }                                                                          \
  } while (0)
#endif


// TODO(docs): Document SingleTimeCommand function
//   - Purpose: Execute Vulkan commands with automatic synchronization
//   - Error handling (returns std::expected)
//   - Performance considerations
//   - Example usage
inline std::expected<void, std::string>
single_time_command(const vk::Device& device, const vk::Queue& queue,
                  const vk::CommandPool& command_pool,
                  const std::function<void(const vk::CommandBuffer&)>& action) {
  vk::CommandBufferAllocateInfo alloc_info(command_pool, vk::CommandBufferLevel::ePrimary, 1);

  auto alloc_res = device.allocateCommandBuffers(alloc_info);
  if (alloc_res.result != vk::Result::eSuccess) {
    return std::unexpected("Failed to allocate command buffers: " + vk::to_string(alloc_res.result));
  }
  vk::CommandBuffer command_buffers = alloc_res.value.front();

  vk::CommandBufferBeginInfo begin_info(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
  if (auto res = command_buffers.begin(begin_info); res != vk::Result::eSuccess) {
    device.freeCommandBuffers(command_pool, command_buffers);
    return std::unexpected("Failed to begin cmdBuffer: " + vk::to_string(res));
  }

  action(command_buffers);

  if (auto res = command_buffers.end(); res != vk::Result::eSuccess) {
    device.freeCommandBuffers(command_pool, command_buffers);
    return std::unexpected("Failed to end cmdbuffer: " + vk::to_string(res));
  }

  auto fence_res = device.createFenceUnique({});
  if (fence_res.result != vk::Result::eSuccess) {
    device.freeCommandBuffers(command_pool, command_buffers);
    return std::unexpected("Failed to create fence: " + vk::to_string(fence_res.result));
  }
  vk::UniqueFence fence = std::move(fence_res.value);

  vk::SubmitInfo submit_info;
  submit_info.setCommandBuffers(command_buffers);
  if (auto res = queue.submit(submit_info, *fence); res != vk::Result::eSuccess) {
    device.freeCommandBuffers(command_pool, command_buffers);
    return std::unexpected("Failed to submit to queue: " + vk::to_string(res));
  }

  if (auto res = device.waitForFences(*fence, VK_TRUE, std::numeric_limits<uint64_t>::max());
      res != vk::Result::eSuccess) {
    device.freeCommandBuffers(command_pool, command_buffers);
    return std::unexpected("Failed to wait for fence: " + vk::to_string(res));
  }

  device.freeCommandBuffers(command_pool, command_buffers);
  return {};
}


// TODO(docs): Document CreateBuffer function
//   - Purpose: Create a Vulkan buffer with memory allocation
//   - Memory property flags explanation
//   - Note about VMA TODO - this will be deprecated
//   - Example usage
inline std::pair<vk::UniqueBuffer, vk::UniqueDeviceMemory>
create_buffer(const vk::Device& device, const vk::PhysicalDevice& physical_device,
             vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties) {
  // TODO: Use VMA (Vulkan Memory Allocator) instead of manual memory allocation
  // TODO: Create a Buffer abstraction class to handle creation, mapping, and destruction
  vk::BufferCreateInfo buffer_info({}, size, usage, vk::SharingMode::eExclusive);
  vk::UniqueBuffer buffer =
      check_vulkan_res_val(device.createBufferUnique(buffer_info), "Failed to create unique buffer: ");

  vk::MemoryRequirements mem_requirements = device.getBufferMemoryRequirements(*buffer);

  vk::MemoryAllocateInfo allocate_info(
      mem_requirements.size,
      find_memory_type(physical_device, mem_requirements.memoryTypeBits, properties));
  vk::UniqueDeviceMemory buffer_memory = check_vulkan_res_val(
      device.allocateMemoryUnique(allocate_info), "Failed to allocate unique memory for buffer: ");

  check_vulkan_res(device.bindBufferMemory(*buffer, *buffer_memory, 0),
                 "Failed to bind buffer memory");

  return {std::move(buffer), std::move(buffer_memory)};
}

// TODO(docs): Document CreateImage function
//   - Purpose: Create a Vulkan image with memory allocation
//   - Format, tiling, usage parameter guidance
//   - Note about VMA TODO
inline std::pair<vk::UniqueImage, vk::UniqueDeviceMemory>
create_image(const vk::Device& device, const vk::PhysicalDevice& physical_device, uint32_t width,
            uint32_t height, vk::Format format, vk::ImageTiling tiling, vk::ImageUsageFlags usage,
            vk::MemoryPropertyFlags properties) {
  // TODO: Use VMA (Vulkan Memory Allocator) instead of manual memory allocation
  // TODO: Create an Image abstraction class to handle creation, views, and memory
  vk::ImageCreateInfo image_info({}, vk::ImageType::e2D, format, vk::Extent3D(width, height, 1), 1,
                                1, vk::SampleCountFlagBits::e1, tiling, usage,
                                vk::SharingMode::eExclusive);

  vk::UniqueImage image =
      check_vulkan_res_val(device.createImageUnique(image_info), "Failed to create unique image: ");

  vk::MemoryRequirements mem_requirements = device.getImageMemoryRequirements(*image);
  vk::MemoryAllocateInfo allocate_info(
      mem_requirements.size,
      find_memory_type(physical_device, mem_requirements.memoryTypeBits, properties));
  vk::UniqueDeviceMemory image_memory = check_vulkan_res_val(device.allocateMemoryUnique(allocate_info),
                                                         "Failed to allocate unique memory:");

  check_vulkan_res(device.bindImageMemory(*image, *image_memory, 0), "Failed to bind image memory: ");

  return {std::move(image), std::move(image_memory)};
}

// TODO(docs): Document CopyBufferToImage function
//   - Purpose: Copy buffer data to an image via command buffer
//   - Requires image to be in eTransferDstOptimal layout
inline void copy_buffer_to_image(const vk::CommandBuffer& cmd_buffer, const vk::Buffer& buffer,
                              const vk::Image& image, uint32_t width, uint32_t height) {
  vk::BufferImageCopy region(0, 0, 0,
                             vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1),
                             vk::Offset3D(0, 0, 0), vk::Extent3D(width, height, 1));
  cmd_buffer.copyBufferToImage(buffer, image, vk::ImageLayout::eTransferDstOptimal, region);
}

// TODO(docs): Document TransitionImageLayout function
//   - Purpose: Transition image layout with pipeline barrier
//   - Supported transitions (Undefined->TransferDst, TransferDst->ShaderReadOnly)
//   - Pipeline stage and access mask logic
//   - Note about ImageMemoryBarrier2 TODO
inline void transition_image_layout(const vk::CommandBuffer& cmd_buffer, const vk::Image& image,
                                  [[maybe_unused]] vk::Format format, vk::ImageLayout old_layout,
                                  vk::ImageLayout new_layout) {
  // TODO: Use vk::ImageMemoryBarrier2 for better synchronization (requires Vulkan 1.3 or extension)
  vk::ImageMemoryBarrier barrier;
  barrier.oldLayout = old_layout;
  barrier.newLayout = new_layout;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.image = image;
  barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
  barrier.subresourceRange.baseMipLevel = 0;
  barrier.subresourceRange.levelCount = 1;
  barrier.subresourceRange.baseArrayLayer = 0;
  barrier.subresourceRange.layerCount = 1;

  vk::PipelineStageFlags source_stage;
  vk::PipelineStageFlags destination_stage;

  if (old_layout == vk::ImageLayout::eUndefined &&
      new_layout == vk::ImageLayout::eTransferDstOptimal) {
    barrier.srcAccessMask = {};
    barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;

    source_stage = vk::PipelineStageFlagBits::eTopOfPipe;
    destination_stage = vk::PipelineStageFlagBits::eTransfer;
  } else if (old_layout == vk::ImageLayout::eTransferDstOptimal &&
             new_layout == vk::ImageLayout::eShaderReadOnlyOptimal) {
    barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
    barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

    source_stage = vk::PipelineStageFlagBits::eTransfer;
    destination_stage = vk::PipelineStageFlagBits::eFragmentShader;
  } else {
    engine_abort("unsupported layout transition!");
  }

  cmd_buffer.pipelineBarrier(source_stage, destination_stage, {}, nullptr, nullptr, barrier);
}


// TODO(docs): Document Texture struct
//   - Purpose: Simple texture container (image + memory + view)
//   - Note about Texture class TODO - this will be replaced
//   - Ownership semantics
struct Texture {
  vk::UniqueImage image;
  vk::UniqueDeviceMemory image_memory;
  vk::UniqueImageView image_view;
};

// TODO(docs): Document CreateTexture function
//   - Purpose: Load a texture from file (STB image + Vulkan upload)
//   - Format (R8G8B8A8Srgb) and why
//   - Error handling (file not found, Vulkan errors)
//   - Performance notes (staging buffer)
//   - Example usage
inline std::expected<Texture, std::string> create_texture(const vk::Device& device,
                                                         const vk::PhysicalDevice& physical_device,
                                                         const vk::Queue& graphics_queue,
                                                         const vk::CommandPool& command_pool,
                                                         const std::filesystem::path& file_path) {
  int tex_width, tex_height, tex_channels;
  stbi_uc* pixels =
      stbi_load(file_path.c_str(), &tex_width, &tex_height, &tex_channels, STBI_rgb_alpha);
  const vk::DeviceSize image_size = tex_width * tex_height * 4;

  if (!pixels) {
    return std::unexpected("Failed to load texture image: " + file_path.string());
  }

  auto [staging_buffer, staging_buffer_memory] = create_buffer(
      device, physical_device, image_size, vk::BufferUsageFlagBits::eTransferSrc,
      vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

  void* data = check_vulkan_res_val(device.mapMemory(*staging_buffer_memory, 0, image_size),
                                 "Failed to map texture image: ");

  memcpy(data, pixels, image_size);
  device.unmapMemory(*staging_buffer_memory);

  stbi_image_free(pixels);

  auto [image, image_memory] =
      create_image(device, physical_device, tex_width, tex_height, vk::Format::eR8G8B8A8Srgb,
                  vk::ImageTiling::eOptimal,
                  vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
                  vk::MemoryPropertyFlagBits::eDeviceLocal);

  auto cmd_res = single_time_command(
      device, graphics_queue, command_pool, [&](const vk::CommandBuffer& cmdBuffer) {
        transition_image_layout(cmdBuffer, *image, vk::Format::eR8G8B8A8Srgb,
                              vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);
        copy_buffer_to_image(cmdBuffer, *staging_buffer, *image, static_cast<uint32_t>(tex_width),
                          static_cast<uint32_t>(tex_height));
        transition_image_layout(cmdBuffer, *image, vk::Format::eR8G8B8A8Srgb,
                              vk::ImageLayout::eTransferDstOptimal,
                              vk::ImageLayout::eShaderReadOnlyOptimal);
      });

  if (!cmd_res) {
    return std::unexpected(cmd_res.error());
  }

  vk::ImageViewCreateInfo view_info({}, *image, vk::ImageViewType::e2D, vk::Format::eR8G8B8A8Srgb,
                                   {}, {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1});
  vk::UniqueImageView image_view = check_vulkan_res_val(device.createImageViewUnique(view_info),
                                                    "Failed to create unique image view: ");

  return Texture{std::move(image), std::move(image_memory), std::move(image_view)};
}

/**
 * Create a Vulkan shader module from SPIR-V bytecode.
 *
 * @note The input buffer must be 4-byte aligned. SPIR-V files from DXC are,
 *       but if loading from arbitrary sources, ensure alignment before calling.
 */
inline vk::UniqueShaderModule create_shader_module(const vk::Device& device,
                                                 const std::vector<char>& code) {
  SD_ALWAYS_ASSERT(code.size() % 4 == 0, "SPIR-V code size must be a multiple of 4");
  vk::ShaderModuleCreateInfo create_info({}, code.size(),
                                        reinterpret_cast<const uint32_t*>(code.data()));
  return check_vulkan_res_val(device.createShaderModuleUnique(create_info),
                           "Failed to create unique shaderModule: ");
}
} // namespace sd

// TODO(docs): Add file-level Doxygen header
//   - @file Utils.hpp
//   - @brief Vulkan utility functions and macros
//   - Note: This is a "kitchen sink" file - consider splitting
//   - Categories: Error checking, buffer/image creation, texture loading
#pragma once
#include <expected>
#include <filesystem>
#include <iostream>
#include <limits>
#include <source_location>
#include <spdlog/spdlog.h>
#include <stb_image.h>

#include "Core/Base.hpp"
#include "Core/Logging.hpp"
#include "Core/Vulkan/VulkanConfig.hpp"
#include "Core/types.hpp"

/**
 * @namespace  Engine
 * @brief Namespace for Engine intrinsic things
 */
namespace SD {


// TODO(docs): Document VK_CHECK macro
//   - Purpose: Vulkan error checking with abort on failure
//   - When to use vs CheckVulkanRes
//   - Example usage
#define VK_CHECK(x)               \
  do {                            \
    VkResult res = (x);           \
    if (res != VK_SUCCESS)        \
      Abort("Vulkan error: " #x); \
  } while (0)

// TODO(docs): Document SD_CORE_ASSERT and SD_ASSERT macros
//   - Difference between debug and release behavior
//   - When to use assertions vs error handling
#ifdef NDEBUG
#define SD_CORE_ASSERT(x, ...) ((void)0)
#else
#define SD_CORE_ASSERT(x, ...)                                   \
  do {                                                           \
    if (!(x)) {                                                  \
      SD::Log::Error("ASSERT {}:{} {}", __FILE__, __LINE__, #x); \
      SD::Abort();                                               \
    }                                                            \
  } while (0)
#endif

#define SD_ASSERT(x, ...)                                        \
  do {                                                           \
    if (!(x)) {                                                  \
      SD::Log::Error("ASSERT {}:{} {}", __FILE__, __LINE__, #x); \
      SD::Abort();                                               \
    }                                                            \
  } while (0)


// TODO(docs): Document SingleTimeCommand function
//   - Purpose: Execute Vulkan commands with automatic synchronization
//   - Error handling (returns std::expected)
//   - Performance considerations
//   - Example usage
inline std::expected<void, std::string>
SingleTimeCommand(const vk::Device& device, const vk::Queue& queue,
                  const vk::CommandPool& commandPool,
                  const std::function<void(const vk::CommandBuffer&)>& action) {
  vk::CommandBufferAllocateInfo allocInfo(commandPool, vk::CommandBufferLevel::ePrimary, 1);

  auto allocRes = device.allocateCommandBuffers(allocInfo);
  if (allocRes.result != vk::Result::eSuccess) {
    return std::unexpected("Failed to allocate command buffers: " + vk::to_string(allocRes.result));
  }
  vk::CommandBuffer cmdBuffer = allocRes.value.front();

  vk::CommandBufferBeginInfo beginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
  if (auto res = cmdBuffer.begin(beginInfo); res != vk::Result::eSuccess) {
    device.freeCommandBuffers(commandPool, cmdBuffer);
    return std::unexpected("Failed to begin cmdBuffer: " + vk::to_string(res));
  }

  action(cmdBuffer);

  if (auto res = cmdBuffer.end(); res != vk::Result::eSuccess) {
    device.freeCommandBuffers(commandPool, cmdBuffer);
    return std::unexpected("Failed to end cmdbuffer: " + vk::to_string(res));
  }

  auto fenceRes = device.createFenceUnique({});
  if (fenceRes.result != vk::Result::eSuccess) {
    device.freeCommandBuffers(commandPool, cmdBuffer);
    return std::unexpected("Failed to create fence: " + vk::to_string(fenceRes.result));
  }
  vk::UniqueFence fence = std::move(fenceRes.value);

  vk::SubmitInfo submitInfo;
  submitInfo.setCommandBuffers(cmdBuffer);
  if (auto res = queue.submit(submitInfo, *fence); res != vk::Result::eSuccess) {
    device.freeCommandBuffers(commandPool, cmdBuffer);
    return std::unexpected("Failed to submit to queue: " + vk::to_string(res));
  }

  if (auto res = device.waitForFences(*fence, VK_TRUE, std::numeric_limits<uint64_t>::max());
      res != vk::Result::eSuccess) {
    device.freeCommandBuffers(commandPool, cmdBuffer);
    return std::unexpected("Failed to wait for fence: " + vk::to_string(res));
  }

  device.freeCommandBuffers(commandPool, cmdBuffer);
  return {};
}


// TODO(docs): Document CreateBuffer function
//   - Purpose: Create a Vulkan buffer with memory allocation
//   - Memory property flags explanation
//   - Note about VMA TODO - this will be deprecated
//   - Example usage
inline std::pair<vk::UniqueBuffer, vk::UniqueDeviceMemory>
CreateBuffer(const vk::Device& device, const vk::PhysicalDevice& physicalDevice,
             vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties) {
  // TODO: Use VMA (Vulkan Memory Allocator) instead of manual memory allocation
  // TODO: Create a Buffer abstraction class to handle creation, mapping, and destruction
  vk::BufferCreateInfo bufferInfo({}, size, usage, vk::SharingMode::eExclusive);
  vk::UniqueBuffer buffer =
      CheckVulkanResVal(device.createBufferUnique(bufferInfo), "Failed to create unique buffer: ");

  vk::MemoryRequirements memRequirements = device.getBufferMemoryRequirements(*buffer);

  vk::MemoryAllocateInfo allocInfo(
      memRequirements.size,
      FindMemoryType(physicalDevice, memRequirements.memoryTypeBits, properties));
  vk::UniqueDeviceMemory bufferMemory = CheckVulkanResVal(
      device.allocateMemoryUnique(allocInfo), "Failed to allocate unique memory for buffer: ");

  CheckVulkanRes(device.bindBufferMemory(*buffer, *bufferMemory, 0),
                 "Failed to bind buffer memory");

  return {std::move(buffer), std::move(bufferMemory)};
}

// TODO(docs): Document CreateImage function
//   - Purpose: Create a Vulkan image with memory allocation
//   - Format, tiling, usage parameter guidance
//   - Note about VMA TODO
inline std::pair<vk::UniqueImage, vk::UniqueDeviceMemory>
CreateImage(const vk::Device& device, const vk::PhysicalDevice& physicalDevice, uint32_t width,
            uint32_t height, vk::Format format, vk::ImageTiling tiling, vk::ImageUsageFlags usage,
            vk::MemoryPropertyFlags properties) {
  // TODO: Use VMA (Vulkan Memory Allocator) instead of manual memory allocation
  // TODO: Create an Image abstraction class to handle creation, views, and memory
  vk::ImageCreateInfo imageInfo({}, vk::ImageType::e2D, format, vk::Extent3D(width, height, 1), 1,
                                1, vk::SampleCountFlagBits::e1, tiling, usage,
                                vk::SharingMode::eExclusive);

  vk::UniqueImage image =
      CheckVulkanResVal(device.createImageUnique(imageInfo), "Failed to create unique image: ");

  vk::MemoryRequirements memRequirements = device.getImageMemoryRequirements(*image);
  vk::MemoryAllocateInfo allocInfo(
      memRequirements.size,
      FindMemoryType(physicalDevice, memRequirements.memoryTypeBits, properties));
  vk::UniqueDeviceMemory imageMemory = CheckVulkanResVal(device.allocateMemoryUnique(allocInfo),
                                                         "Failed to allocate unique memory:");

  CheckVulkanRes(device.bindImageMemory(*image, *imageMemory, 0), "Failed to bind image memory: ");

  return {std::move(image), std::move(imageMemory)};
}

// TODO(docs): Document CopyBufferToImage function
//   - Purpose: Copy buffer data to an image via command buffer
//   - Requires image to be in eTransferDstOptimal layout
inline void CopyBufferToImage(const vk::CommandBuffer& cmdBuffer, const vk::Buffer& buffer,
                              const vk::Image& image, uint32_t width, uint32_t height) {
  vk::BufferImageCopy region(0, 0, 0,
                             vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1),
                             vk::Offset3D(0, 0, 0), vk::Extent3D(width, height, 1));
  cmdBuffer.copyBufferToImage(buffer, image, vk::ImageLayout::eTransferDstOptimal, region);
}

// TODO(docs): Document TransitionImageLayout function
//   - Purpose: Transition image layout with pipeline barrier
//   - Supported transitions (Undefined->TransferDst, TransferDst->ShaderReadOnly)
//   - Pipeline stage and access mask logic
//   - Note about ImageMemoryBarrier2 TODO
inline void TransitionImageLayout(const vk::CommandBuffer& cmdBuffer, const vk::Image& image,
                                  [[maybe_unused]] vk::Format format, vk::ImageLayout oldLayout,
                                  vk::ImageLayout newLayout) {
  // TODO: Use vk::ImageMemoryBarrier2 for better synchronization (requires Vulkan 1.3 or extension)
  vk::ImageMemoryBarrier barrier;
  barrier.oldLayout = oldLayout;
  barrier.newLayout = newLayout;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.image = image;
  barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
  barrier.subresourceRange.baseMipLevel = 0;
  barrier.subresourceRange.levelCount = 1;
  barrier.subresourceRange.baseArrayLayer = 0;
  barrier.subresourceRange.layerCount = 1;

  vk::PipelineStageFlags sourceStage;
  vk::PipelineStageFlags destinationStage;

  if (oldLayout == vk::ImageLayout::eUndefined &&
      newLayout == vk::ImageLayout::eTransferDstOptimal) {
    barrier.srcAccessMask = {};
    barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;

    sourceStage = vk::PipelineStageFlagBits::eTopOfPipe;
    destinationStage = vk::PipelineStageFlagBits::eTransfer;
  } else if (oldLayout == vk::ImageLayout::eTransferDstOptimal &&
             newLayout == vk::ImageLayout::eShaderReadOnlyOptimal) {
    barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
    barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

    sourceStage = vk::PipelineStageFlagBits::eTransfer;
    destinationStage = vk::PipelineStageFlagBits::eFragmentShader;
  } else {
    Abort("unsupported layout transition!");
  }

  cmdBuffer.pipelineBarrier(sourceStage, destinationStage, {}, nullptr, nullptr, barrier);
}


// TODO(docs): Document Texture struct
//   - Purpose: Simple texture container (image + memory + view)
//   - Note about Texture class TODO - this will be replaced
//   - Ownership semantics
struct Texture {
  vk::UniqueImage image;
  vk::UniqueDeviceMemory imageMemory;
  vk::UniqueImageView imageView;
};

// TODO(docs): Document CreateTexture function
//   - Purpose: Load a texture from file (STB image + Vulkan upload)
//   - Format (R8G8B8A8Srgb) and why
//   - Error handling (file not found, Vulkan errors)
//   - Performance notes (staging buffer)
//   - Example usage
inline std::expected<Texture, std::string> CreateTexture(const vk::Device& device,
                                                         const vk::PhysicalDevice& physicalDevice,
                                                         const vk::Queue& graphicsQueue,
                                                         const vk::CommandPool& commandPool,
                                                         const std::filesystem::path& filePath) {
  int texWidth, texHeight, texChannels;
  stbi_uc* pixels =
      stbi_load(filePath.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
  vk::DeviceSize imageSize = texWidth * texHeight * 4;

  if (!pixels) {
    return std::unexpected("Failed to load texture image: " + filePath.string());
  }

  auto [stagingBuffer, stagingBufferMemory] = CreateBuffer(
      device, physicalDevice, imageSize, vk::BufferUsageFlagBits::eTransferSrc,
      vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

  void* data = CheckVulkanResVal(device.mapMemory(*stagingBufferMemory, 0, imageSize),
                                 "Failed to map texture image: ");

  memcpy(data, pixels, imageSize);
  device.unmapMemory(*stagingBufferMemory);

  stbi_image_free(pixels);

  auto [image, imageMemory] =
      CreateImage(device, physicalDevice, texWidth, texHeight, vk::Format::eR8G8B8A8Srgb,
                  vk::ImageTiling::eOptimal,
                  vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
                  vk::MemoryPropertyFlagBits::eDeviceLocal);

  auto cmdRes = SingleTimeCommand(
      device, graphicsQueue, commandPool, [&](const vk::CommandBuffer& cmdBuffer) {
        TransitionImageLayout(cmdBuffer, *image, vk::Format::eR8G8B8A8Srgb,
                              vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);
        CopyBufferToImage(cmdBuffer, *stagingBuffer, *image, static_cast<uint32_t>(texWidth),
                          static_cast<uint32_t>(texHeight));
        TransitionImageLayout(cmdBuffer, *image, vk::Format::eR8G8B8A8Srgb,
                              vk::ImageLayout::eTransferDstOptimal,
                              vk::ImageLayout::eShaderReadOnlyOptimal);
      });

  if (!cmdRes) {
    return std::unexpected(cmdRes.error());
  }

  vk::ImageViewCreateInfo viewInfo({}, *image, vk::ImageViewType::e2D, vk::Format::eR8G8B8A8Srgb,
                                   {}, {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1});
  vk::UniqueImageView imageView = CheckVulkanResVal(device.createImageViewUnique(viewInfo),
                                                    "Failed to create unique image view: ");

  return Texture{std::move(image), std::move(imageMemory), std::move(imageView)};
}

// TODO(docs): Document CreateShaderModule function
//   - Purpose: Create a Vulkan shader module from SPIR-V bytecode
//   - Input format (vector<char> of SPIR-V)
//   - Example usage with shader loading
inline vk::UniqueShaderModule CreateShaderModule(const vk::Device& device,
                                                 const std::vector<char>& code) {
  vk::ShaderModuleCreateInfo createInfo({}, code.size(),
                                        reinterpret_cast<const uint32_t*>(code.data()));
  return CheckVulkanResVal(device.createShaderModuleUnique(createInfo),
                           "Failed to create unique shaderModule: ");
}
} // namespace SD

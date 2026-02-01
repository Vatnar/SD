#pragma once
#include <expected>
#include <filesystem>
#include <iostream>
#include <source_location>
#include <spdlog/spdlog.h>
#include <stb_image.h>

#include "Core/VulkanConfig.hpp"

namespace Engine {

[[noreturn]] inline void Abort(const std::string& message) {
  if (const auto logger = spdlog::get("engine")) {
    logger->critical("Fatal error: {}", message);
    spdlog::shutdown();
  } else {
    std::cerr << "Fatal error: " << message << '\n';
  }
  std::exit(EXIT_FAILURE);
}
} // namespace Engine


// TODO: prolly make this print locations,
inline void CheckVulkanRes(vk::Result result, std::string_view message,
                           std::source_location loc = std::source_location::current()) {
  if (result != vk::Result::eSuccess) {
    auto p =
        std::format("{}:{} {}: {}", loc.file_name(), loc.line(), message, vk::to_string(result));
    Engine::Abort(p);
  }
}

template<typename T>
T CheckVulkanResVal(vk::ResultValue<T>&& result, std::string_view message,
                    std::source_location loc = std::source_location::current()) {
  CheckVulkanRes(result.result, message, loc);

  // NOTE: We need to use an explicit move here to guarantee no calling of deleted constructors
  return std::move(result.value);
}


inline void SingleTimeCommand(const vk::Device& device, const vk::Queue& queue,
                              const vk::CommandPool& commandPool,
                              const std::function<void(const vk::CommandBuffer&)>& action) {
  vk::CommandBufferAllocateInfo allocInfo(commandPool, vk::CommandBufferLevel::ePrimary, 1);

  vk::CommandBuffer cmdBuffer =
      CheckVulkanResVal(device.allocateCommandBuffers(allocInfo), "Pipeline creation failed: ")
          .front();

  vk::CommandBufferBeginInfo beginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
  CheckVulkanRes(cmdBuffer.begin(beginInfo), "Failed to begin cmdBuffer:");

  action(cmdBuffer);

  CheckVulkanRes(cmdBuffer.end(), "Failed to end cmdbuffer:");

  vk::SubmitInfo submitInfo;
  submitInfo.setCommandBuffers(cmdBuffer);
  CheckVulkanRes(queue.submit(submitInfo, nullptr), "Failed to submit to queue:");

  CheckVulkanRes(queue.waitIdle(), "Failed to wait for queue");

  device.freeCommandBuffers(commandPool, cmdBuffer);
}

inline uint32_t FindMemoryType(const vk::PhysicalDevice& physicalDevice, uint32_t typeFilter,
                               vk::MemoryPropertyFlags properties) {
  vk::PhysicalDeviceMemoryProperties memProperties = physicalDevice.getMemoryProperties();

  for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
    if ((typeFilter & (1 << i)) &&
        (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
      return i;
    }
  }
  Engine::Abort("failed to find suitable memory type!");
}


inline std::pair<vk::UniqueBuffer, vk::UniqueDeviceMemory>
CreateBuffer(const vk::Device& device, const vk::PhysicalDevice& physicalDevice,
             vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties) {
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

inline std::pair<vk::UniqueImage, vk::UniqueDeviceMemory>
CreateImage(const vk::Device& device, const vk::PhysicalDevice& physicalDevice, uint32_t width,
            uint32_t height, vk::Format format, vk::ImageTiling tiling, vk::ImageUsageFlags usage,
            vk::MemoryPropertyFlags properties) {
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

inline void CopyBufferToImage(const vk::CommandBuffer& cmdBuffer, const vk::Buffer& buffer,
                              const vk::Image& image, uint32_t width, uint32_t height) {
  vk::BufferImageCopy region(0, 0, 0,
                             vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1),
                             vk::Offset3D(0, 0, 0), vk::Extent3D(width, height, 1));
  cmdBuffer.copyBufferToImage(buffer, image, vk::ImageLayout::eTransferDstOptimal, region);
}

inline void TransitionImageLayout(const vk::CommandBuffer& cmdBuffer, const vk::Image& image,
                                  vk::Format format, vk::ImageLayout oldLayout,
                                  vk::ImageLayout newLayout) {
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
    Engine::Abort("unsupported layout transition!");
  }

  cmdBuffer.pipelineBarrier(sourceStage, destinationStage, {}, nullptr, nullptr, barrier);
}


struct Texture {
  vk::UniqueImage image;
  vk::UniqueDeviceMemory imageMemory;
  vk::UniqueImageView imageView;
};

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

  SingleTimeCommand(device, graphicsQueue, commandPool, [&](const vk::CommandBuffer& cmdBuffer) {
    TransitionImageLayout(cmdBuffer, *image, vk::Format::eR8G8B8A8Srgb, vk::ImageLayout::eUndefined,
                          vk::ImageLayout::eTransferDstOptimal);
    CopyBufferToImage(cmdBuffer, *stagingBuffer, *image, static_cast<uint32_t>(texWidth),
                      static_cast<uint32_t>(texHeight));
    TransitionImageLayout(cmdBuffer, *image, vk::Format::eR8G8B8A8Srgb,
                          vk::ImageLayout::eTransferDstOptimal,
                          vk::ImageLayout::eShaderReadOnlyOptimal);
  });

  vk::ImageViewCreateInfo viewInfo({}, *image, vk::ImageViewType::e2D, vk::Format::eR8G8B8A8Srgb,
                                   {}, {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1});
  vk::UniqueImageView imageView = CheckVulkanResVal(device.createImageViewUnique(viewInfo),
                                                    "Failed to create unique image view: ");

  return Texture{std::move(image), std::move(imageMemory), std::move(imageView)};
}

inline vk::UniqueShaderModule CreateShaderModule(const vk::Device& device,
                                                 const std::vector<char>& code) {
  vk::ShaderModuleCreateInfo createInfo({}, code.size(),
                                        reinterpret_cast<const uint32_t*>(code.data()));
  return CheckVulkanResVal(device.createShaderModuleUnique(createInfo),
                           "Failed to create unique shaderModule: ");
}

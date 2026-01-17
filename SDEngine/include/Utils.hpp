#pragma once
#include "VulkanConfig.hpp"
#include "stb_image.h"
// #include "nvrhi/nvrhi.h"
#include <expected>
#include <filesystem>
#include <iostream>

#include "spdlog/spdlog.h"

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

template<class T>
void CheckVulkanRes() {
}


inline void SingleTimeCommand(const vk::Device& device, const vk::Queue& queue,
                              const vk::CommandPool& commandPool,
                              const std::function<void(const vk::CommandBuffer&)>& action) {
  vk::CommandBufferAllocateInfo allocInfo(commandPool, vk::CommandBufferLevel::ePrimary, 1);

  vk::CommandBuffer cmdBuffer;
  {
    auto result = device.allocateCommandBuffers(allocInfo);
    if (result.result != vk::Result::eSuccess) {
      Engine::Abort("Pipeline creation failed: " + vk::to_string(result.result));
    }
    cmdBuffer = result.value.front();
  }


  vk::CommandBufferBeginInfo beginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
  if (auto result = cmdBuffer.begin(beginInfo); result != vk::Result::eSuccess) {
    Engine::Abort("Pipeline creation failed: " + vk::to_string(result));
  }

  action(cmdBuffer);

  if (auto result = cmdBuffer.end() = c)
    cmdBuffer.end();

  vk::SubmitInfo submitInfo;
  submitInfo.setCommandBuffers(cmdBuffer);
  queue.submit(submitInfo, nullptr);
  queue.waitIdle();

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
  vk::UniqueBuffer buffer = device.createBufferUnique(bufferInfo);

  vk::MemoryRequirements memRequirements = device.getBufferMemoryRequirements(*buffer);

  vk::MemoryAllocateInfo allocInfo(
      memRequirements.size,
      FindMemoryType(physicalDevice, memRequirements.memoryTypeBits, properties));
  vk::UniqueDeviceMemory bufferMemory = device.allocateMemoryUnique(allocInfo);

  device.bindBufferMemory(*buffer, *bufferMemory, 0);

  return {std::move(buffer), std::move(bufferMemory)};
}

inline std::pair<vk::UniqueImage, vk::UniqueDeviceMemory>
CreateImage(const vk::Device& device, const vk::PhysicalDevice& physicalDevice, uint32_t width,
            uint32_t height, vk::Format format, vk::ImageTiling tiling, vk::ImageUsageFlags usage,
            vk::MemoryPropertyFlags properties) {
  vk::ImageCreateInfo imageInfo({}, vk::ImageType::e2D, format, vk::Extent3D(width, height, 1), 1,
                                1, vk::SampleCountFlagBits::e1, tiling, usage,
                                vk::SharingMode::eExclusive);

  vk::UniqueImage image = device.createImageUnique(imageInfo);

  vk::MemoryRequirements memRequirements = device.getImageMemoryRequirements(*image);
  vk::MemoryAllocateInfo allocInfo(
      memRequirements.size,
      FindMemoryType(physicalDevice, memRequirements.memoryTypeBits, properties));
  vk::UniqueDeviceMemory imageMemory = device.allocateMemoryUnique(allocInfo);

  device.bindImageMemory(*image, *imageMemory, 0);

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

  void* data = device.mapMemory(*stagingBufferMemory, 0, imageSize);
  memcpy(data, pixels, static_cast<size_t>(imageSize));
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
  vk::UniqueImageView imageView = device.createImageViewUnique(viewInfo);

  return Texture{std::move(image), std::move(imageMemory), std::move(imageView)};
}

inline vk::UniqueShaderModule CreateShaderModule(const vk::Device& device,
                                                 const std::vector<char>& code) {
  vk::ShaderModuleCreateInfo createInfo({}, code.size(),
                                        reinterpret_cast<const uint32_t*>(code.data()));
  return device.createShaderModuleUnique(createInfo);
}

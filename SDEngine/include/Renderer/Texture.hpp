#pragma once

#include "Core/VulkanConfig.hpp"
#include <memory>

// TODO: This is a basic texture abstraction. A more complete version would handle
// different texture types (2D, 3D, cubemaps), formats, and mipmapping.

class Texture {
public:
    // For simplicity, this just holds the Vulkan handles.
    // In a real implementation, these would be wrapped in an Image class.
    vk::UniqueImage image;
    vk::UniqueDeviceMemory imageMemory;
    vk::UniqueImageView imageView;
    vk::UniqueSampler sampler; // Sampler could also be a separate object shared by multiple textures.
};

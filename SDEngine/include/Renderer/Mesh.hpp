#pragma once

#include "Core/VulkanConfig.hpp"
#include <memory>

// TODO: This is a very basic mesh abstraction. A more complete version would also handle
// index buffers, different vertex layouts, and potentially skinning/animation data.

class Mesh {
public:
    // For simplicity, this just holds the Vulkan buffer handles.
    // In a real implementation, these would be wrapped in a Buffer class.
    vk::UniqueBuffer vertexBuffer;
    vk::UniqueDeviceMemory vertexBufferMemory;
    uint32_t vertexCount;
};

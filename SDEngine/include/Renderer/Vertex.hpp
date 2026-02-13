#pragma once
#include "Core/VulkanConfig.hpp"

struct Vertex {
  float position[3];
  float texCoord[2];

  static vk::VertexInputBindingDescription getBindingDescription();

  static std::array<vk::VertexInputAttributeDescription, 2> getAttributeDescriptions();
};

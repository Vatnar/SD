#pragma once
#include <VLA/Vector.hpp>
#include <vulkan/vulkan.hpp>

#include "core/types.hpp"
namespace sd {

struct VertexPNUV {
  VLA::Vector3f position;
  VLA::Vector3f normal;
  VLA::Vector2f uv;

  static constexpr std::array<vk::VertexInputBindingDescription, 1> binding_descriptions() {
    return {
        {{.binding = 0, .stride = sizeof(VertexPNUV), .inputRate = vk::VertexInputRate::eVertex}}};
  }

  static constexpr std::array<vk::VertexInputAttributeDescription, 3> attribute_descriptions() {
    return {
        {
         {
                .location = 0,
                .binding  = 0,
                .format   = vk::Format::eR32G32B32Sfloat,
                .offset   = static_cast<U32>(offsetof(VertexPNUV, position)),
            }, {
                .location = 1,
                .binding  = 0,
                .format   = vk::Format::eR32G32B32Sfloat,
                .offset   = static_cast<U32>(offsetof(VertexPNUV, normal)),
            }, {
                .location = 2,
                .binding  = 0,
                .format   = vk::Format::eR32G32Sfloat,
                .offset   = static_cast<U32>(offsetof(VertexPNUV, uv)),
            }, }
    };
  };
};
} // namespace sd

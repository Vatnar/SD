#pragma once
#include <VLA/Matrix.hpp>

#include "Component.hpp"
#include "Core/types.hpp"

namespace SD {

struct Transform {
  VLA::Matrix4x4f worldMatrix;
};
REGISTER_SD_COMPONENT(Transform);
struct Camera {
  VLA::Matrix4x4f view;
  VLA::Matrix4x4f proj;
};
REGISTER_SD_COMPONENT(Camera);

struct Renderable {
  u32 meshId;
  u32 materialId;
  i32 renderStage;
  u32 viewMask; // which view buffer to render to

  float color[4] = {1.0f, 0.0f, 0.0f, 1.0f}; // Default red for debugging
};
REGISTER_SD_COMPONENT(Renderable);

struct DebugName {
  std::string name;
};
REGISTER_SD_COMPONENT(DebugName);
} // namespace SD

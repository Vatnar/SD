#pragma once
#include <VLA/Matrix.hpp>

#include "Component.hpp"
#include "Core/types.hpp"

struct Camera {
  VLA::Matrix4x4f view;
  VLA::Matrix4x4f proj;
};

struct Renderable {
  u32 me
};

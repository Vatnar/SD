#pragma once
#include <VLA/Matrix.hpp>

#include "Component.hpp"
#include "Core/types.hpp"

namespace SD {

struct Transform {
  VLA::Matrix4x4f worldMatrix;
};
REGISTER_SD_COMPONENT(Transform);

template<>
struct ComponentSerializer<Transform> {
  static void Serialize(const Transform& c, Serializer& s) { s.Write(c.worldMatrix.A); }
  static void Deserialize(Transform& c, Serializer& s) { s.Read(c.worldMatrix.A); }
};

struct Camera {
  VLA::Matrix4x4f view;
  VLA::Matrix4x4f proj;
};
REGISTER_SD_COMPONENT(Camera);

template<>
struct ComponentSerializer<Camera> {
  static void Serialize(const Camera& c, Serializer& s) {
    s.Write(c.view.A);
    s.Write(c.proj.A);
  }
  static void Deserialize(Camera& c, Serializer& s) {
    s.Read(c.view.A);
    s.Read(c.proj.A);
  }
};

struct Renderable {
  u32 meshId = 0;
  u32 materialId = 0;
  i32 renderStage = 0;
  u32 viewMask = 0;
  float color[4] = {1.0f, 0.0f, 0.0f, 1.0f};
};
REGISTER_SD_COMPONENT(Renderable);

template<>
struct ComponentSerializer<Renderable> {
  static void Serialize(const Renderable& c, Serializer& s) {
    s.Write(c.meshId);
    s.Write(c.materialId);
    s.Write(c.renderStage);
    s.Write(c.viewMask);
    s.Write(c.color);
  }
  static void Deserialize(Renderable& c, Serializer& s) {
    c.meshId = s.Read<u32>();
    c.materialId = s.Read<u32>();
    c.renderStage = s.Read<i32>();
    c.viewMask = s.Read<u32>();
    s.Read(c.color);
  }
};

struct DebugName {
  std::string name;
};
REGISTER_SD_COMPONENT(DebugName);

template<>
struct ComponentSerializer<DebugName> {
  static void Serialize(const DebugName& c, Serializer& s) { s.Write(c.name); }
  static void Deserialize(DebugName& c, Serializer& s) { c.name = s.ReadString(); }
};

} // namespace SD

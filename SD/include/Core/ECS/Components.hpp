#pragma once
#include <VLA/Matrix.hpp>

#include "Component.hpp"
#include "ComponentFactory.hpp"
#include "Core/types.hpp"

namespace sd {

struct Transform {
  VLA::Matrix4x4f world_matrix;
};
REGISTER_SD_COMPONENT(Transform);
REGISTER_SERIALIZABLE_COMPONENT(Transform);

template<>
struct ComponentSerializer<Transform> {
  static void serialize(const Transform& c, Serializer& s) { s.write(c.world_matrix.A); }
  static void deserialize(Transform& c, Serializer& s) { s.read(c.world_matrix.A); }
};

struct Camera {
  VLA::Matrix4x4f view;
  VLA::Matrix4x4f proj;
};
REGISTER_SD_COMPONENT(Camera);
REGISTER_SERIALIZABLE_COMPONENT(Camera);

template<>
struct ComponentSerializer<Camera> {
  static void serialize(const Camera& c, Serializer& s) {
    s.write(c.view.A);
    s.write(c.proj.A);
  }
  static void deserialize(Camera& c, Serializer& s) {
    s.read(c.view.A);
    s.read(c.proj.A);
  }
};

struct Renderable {
  u32 mesh_id = 0;
  u32 material_id = 0;
  i32 render_stage = 0;
  u32 view_mask = 0;
  float color[4] = {1.0f, 0.0f, 0.0f, 1.0f};
};
REGISTER_SD_COMPONENT(Renderable);
REGISTER_SERIALIZABLE_COMPONENT(Renderable);

template<>
struct ComponentSerializer<Renderable> {
  static void serialize(const Renderable& c, Serializer& s) {
    s.write(c.mesh_id);
    s.write(c.material_id);
    s.write(c.render_stage);
    s.write(c.view_mask);
    s.write(c.color);
  }
  static void deserialize(Renderable& c, Serializer& s) {
    c.mesh_id = s.read<u32>();
    c.material_id = s.read<u32>();
    c.render_stage = s.read<i32>();
    c.view_mask = s.read<u32>();
    s.read(c.color);
  }
};

struct DebugName {
  std::string name;
};
REGISTER_SD_COMPONENT(DebugName);
REGISTER_SERIALIZABLE_COMPONENT(DebugName);

template<>
struct ComponentSerializer<DebugName> {
  static void serialize(const DebugName& c, Serializer& s) { s.write(c.name); }
  static void deserialize(DebugName& c, Serializer& s) { c.name = s.read_string(); }
};

} // namespace SD

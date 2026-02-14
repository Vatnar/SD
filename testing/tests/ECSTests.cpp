#include <Core/ECS/Entity.hpp>

#include "Core/ECS/Component.hpp"
#include "Core/ECS/EntityManager.hpp"
#include "Core/Logging.hpp"
#include "Core/Systems/Renderer/Material.hpp"
#include "Core/Systems/Renderer/Mesh.hpp"
#include "VLA/Matrix.hpp"
#include "gtest/gtest.h"
#include "spdlog/fmt/bundled/ostream.h"


struct Renderable {
  std::shared_ptr<Material> material;
};
REGISTER_SD_COMPONENT(Renderable);

struct StaticMesh {
  std::shared_ptr<Mesh> mesh;
};
REGISTER_SD_COMPONENT(StaticMesh);

struct Transform {
  VLA::Matrix4x4f transform;
};
REGISTER_SD_COMPONENT(Transform);

struct RigidBody {
  VLA::Vector3f velocity;
  VLA::Vector3f acceleration;
  VLA::Vector3f force;
  VLA::Vector3f moment;
  VLA::Matrix4x4f transform;
};

class RenderSystem {
  // Do some render stuff based on Renderable data
  void Update(EntityManager& em) {
    // for (auto [entity, renderable]) : em.View<Renderable>()) {
    // }
  }
};


TEST(ECSTest, ReplaceComponent) {
  EntityManager entityManager;
  Entity e = entityManager.Create();

  VLA::Matrix4x4f initialMatrix = VLA::Matrix4x4f::Identity;
  entityManager.AddComponent<Transform>(e, initialMatrix);

  Transform* transPtrBefore = entityManager.TryGetComponent<Transform>(e);
  ASSERT_NE(transPtrBefore, nullptr);
  EXPECT_EQ(transPtrBefore->transform, initialMatrix);

  // Overwrite Component
  VLA::Matrix4x4f newMatrix = VLA::Matrix4x4f::Identity;
  newMatrix(0, 3) = 123.0f;
  newMatrix(1, 3) = 456.0f;
  entityManager.AddComponent<Transform>(e, newMatrix);

  //  Get Pointer Again
  Transform* transPtrAfter = entityManager.TryGetComponent<Transform>(e);

  //  Verify Logic
  // Note: This relies on vector not resizing, which is guaranteed for 1 element.
  EXPECT_EQ(transPtrBefore, transPtrAfter)
      << "Pointer address changed! Was it a push_back instead of overwrite?";

  EXPECT_EQ(transPtrAfter->transform, newMatrix);

  auto components = entityManager.GetAllComponentInfo(e);
  int transformCount = 0;
  for (const auto& cmp : components) {
    if (cmp.id == ComponentTraits<Transform>::Id)
      transformCount++;
  }
  EXPECT_EQ(transformCount, 1) << "Entity reports multiple Transform components!";
}

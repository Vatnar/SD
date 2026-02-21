#include <Core/ECS/Entity.hpp>

#include "Core/ECS/Component.hpp"
#include "Core/ECS/EntityManager.hpp"
#include "Core/Logging.hpp"
#include "VLA/Matrix.hpp"
#include "gtest/gtest.h"
#include "spdlog/fmt/bundled/ostream.h"

namespace SD {
struct Transform {
  VLA::Matrix4x4f transform;
};
REGISTER_SD_COMPONENT(Transform);
} // namespace SD

TEST(ECSTest, ReplaceComponent) {
  SD::EntityManager entityManager;
  SD::Entity e = entityManager.Create();

  VLA::Matrix4x4f initialMatrix = VLA::Matrix4x4f::Identity;
  entityManager.AddComponent<SD::Transform>(e, initialMatrix);

  SD::Transform* transPtrBefore = entityManager.TryGetComponent<SD::Transform>(e);
  ASSERT_NE(transPtrBefore, nullptr);
  EXPECT_EQ(transPtrBefore->transform, initialMatrix);

  // Overwrite Component
  VLA::Matrix4x4f newMatrix = VLA::Matrix4x4f::Identity;
  newMatrix(0, 3) = 123.0f;
  newMatrix(1, 3) = 456.0f;
  entityManager.AddComponent<SD::Transform>(e, newMatrix);

  //  Get Pointer Again
  SD::Transform* transPtrAfter = entityManager.TryGetComponent<SD::Transform>(e);

  //  Verify Logic
  // Note: This relies on vector not resizing, which is guaranteed for 1 element.
  EXPECT_EQ(transPtrBefore, transPtrAfter)
      << "Pointer address changed! Was it a push_back instead of overwrite?";

  EXPECT_EQ(transPtrAfter->transform, newMatrix);

  auto components = entityManager.GetAllComponentInfo(e);
  int transformCount = 0;
  for (const auto& cmp : components) {
    if (cmp.id == SD::ComponentTraits<SD::Transform>::Id)
      transformCount++;
  }
  EXPECT_EQ(transformCount, 1) << "Entity reports multiple Transform components!";
}

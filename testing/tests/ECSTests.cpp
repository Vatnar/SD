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

struct Velocity {
  float x, y, z;
};
REGISTER_SD_COMPONENT(Velocity);

struct Health {
  int current;
  int max;
};
REGISTER_SD_COMPONENT(Health);
} // namespace SD

TEST(ECSTest, ReplaceComponent) {
  SD::EntityManager entityManager;
  SD::Entity e = entityManager.Create();

  VLA::Matrix4x4f initialMatrix = VLA::Matrix4x4f::Identity;
  entityManager.AddComponent<SD::Transform>(e, initialMatrix);

  SD::Transform* transPtrBefore = entityManager.TryGetComponent<SD::Transform>(e);
  ASSERT_NE(transPtrBefore, nullptr);
  EXPECT_EQ(transPtrBefore->transform, initialMatrix);

  VLA::Matrix4x4f newMatrix = VLA::Matrix4x4f::Identity;
  newMatrix(0, 3) = 123.0f;
  newMatrix(1, 3) = 456.0f;
  entityManager.AddComponent<SD::Transform>(e, newMatrix);

  SD::Transform* transPtrAfter = entityManager.TryGetComponent<SD::Transform>(e);

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

class SparseEntitySetTest : public ::testing::Test {
protected:
  SD::SparseEntitySet<SD::Velocity> set;
};

TEST_F(SparseEntitySetTest, AddAndGet_ReturnsCorrectData) {
  SD::Entity e{0, 0};
  set.Add(e, 1.0f, 2.0f, 3.0f);

  SD::Velocity* v = set.Get(e);
  ASSERT_NE(v, nullptr);
  EXPECT_FLOAT_EQ(v->x, 1.0f);
  EXPECT_FLOAT_EQ(v->y, 2.0f);
  EXPECT_FLOAT_EQ(v->z, 3.0f);
}

TEST_F(SparseEntitySetTest, Get_NonExistent_ReturnsNullptr) {
  SD::Entity e{0, 0};
  EXPECT_EQ(set.Get(e), nullptr);
}

TEST_F(SparseEntitySetTest, Remove_Existing_ReturnsTrue) {
  SD::Entity e{0, 0};
  set.Add(e, 1.0f, 2.0f, 3.0f);

  EXPECT_TRUE(set.Remove(e));
  EXPECT_EQ(set.Get(e), nullptr);
  EXPECT_EQ(set.Size(), 0u);
}

TEST_F(SparseEntitySetTest, Remove_NonExistent_ReturnsFalse) {
  SD::Entity e{0, 0};
  EXPECT_FALSE(set.Remove(e));
}

TEST_F(SparseEntitySetTest, Remove_WrongGeneration_ReturnsFalse) {
  SD::Entity e1{0, 0};
  set.Add(e1, 1.0f, 2.0f, 3.0f);

  SD::Entity e2{0, 1};
  EXPECT_FALSE(set.Remove(e2));
  EXPECT_NE(set.Get(e1), nullptr);
}

TEST_F(SparseEntitySetTest, Remove_SwapCorrectness) {
  SD::Entity e0{0, 0};
  SD::Entity e1{1, 0};
  SD::Entity e2{2, 0};

  set.Add(e0, 10.0f, 0.0f, 0.0f);
  set.Add(e1, 20.0f, 0.0f, 0.0f);
  set.Add(e2, 30.0f, 0.0f, 0.0f);

  ASSERT_EQ(set.Size(), 3u);

  EXPECT_TRUE(set.Remove(e0));

  EXPECT_EQ(set.Size(), 2u);
  EXPECT_EQ(set.Get(e0), nullptr);

  SD::Velocity* v1 = set.Get(e1);
  SD::Velocity* v2 = set.Get(e2);

  ASSERT_NE(v1, nullptr);
  ASSERT_NE(v2, nullptr);
  EXPECT_FLOAT_EQ(v1->x, 20.0f);
  EXPECT_FLOAT_EQ(v2->x, 30.0f);
}

TEST_F(SparseEntitySetTest, Remove_MiddleElement_MaintainsIntegrity) {
  SD::Entity entities[5] = {{0, 0}, {1, 0}, {2, 0}, {3, 0}, {4, 0}};

  for (int i = 0; i < 5; ++i) {
    set.Add(entities[i], static_cast<float>(i * 10), 0.0f, 0.0f);
  }

  EXPECT_TRUE(set.Remove(entities[2]));

  for (int i = 0; i < 5; ++i) {
    SD::Velocity* v = set.Get(entities[i]);
    if (i == 2) {
      EXPECT_EQ(v, nullptr);
    } else {
      ASSERT_NE(v, nullptr) << "Entity " << i << " should still exist";
      EXPECT_FLOAT_EQ(v->x, static_cast<float>(i * 10));
    }
  }
}

TEST_F(SparseEntitySetTest, SerializeDeserialize_RoundTrip) {
  SD::Entity e0{0, 0};
  SD::Entity e1{100, 0};
  SD::Entity e2{2048, 0};

  set.Add(e0, 1.0f, 2.0f, 3.0f);
  set.Add(e1, 10.0f, 20.0f, 30.0f);
  set.Add(e2, 100.0f, 200.0f, 300.0f);

  std::vector<char> buffer;
  set.SerializeTo(buffer);

  SD::SparseEntitySet<SD::Velocity> deserialized;
  deserialized.DeserializeFrom(buffer);

  EXPECT_EQ(deserialized.Size(), 3u);

  SD::Velocity* v0 = deserialized.Get(e0);
  SD::Velocity* v1 = deserialized.Get(e1);
  SD::Velocity* v2 = deserialized.Get(e2);

  ASSERT_NE(v0, nullptr);
  ASSERT_NE(v1, nullptr);
  ASSERT_NE(v2, nullptr);

  EXPECT_FLOAT_EQ(v0->x, 1.0f);
  EXPECT_FLOAT_EQ(v0->y, 2.0f);
  EXPECT_FLOAT_EQ(v0->z, 3.0f);

  EXPECT_FLOAT_EQ(v1->x, 10.0f);
  EXPECT_FLOAT_EQ(v1->y, 20.0f);
  EXPECT_FLOAT_EQ(v1->z, 30.0f);

  EXPECT_FLOAT_EQ(v2->x, 100.0f);
  EXPECT_FLOAT_EQ(v2->y, 200.0f);
  EXPECT_FLOAT_EQ(v2->z, 300.0f);
}

TEST_F(SparseEntitySetTest, SerializeDeserialize_EmptySet) {
  std::vector<char> buffer;
  set.SerializeTo(buffer);

  SD::SparseEntitySet<SD::Velocity> deserialized;
  deserialized.DeserializeFrom(buffer);

  EXPECT_EQ(deserialized.Size(), 0u);
}

class EntityManagerTest : public ::testing::Test {
protected:
  SD::EntityManager manager;
};

TEST_F(EntityManagerTest, Create_ReturnsAliveEntity) {
  SD::Entity e = manager.Create();
  EXPECT_TRUE(manager.IsAlive(e));
}

TEST_F(EntityManagerTest, Destroy_EntityNoLongerAlive) {
  SD::Entity e = manager.Create();
  manager.Destroy(e);
  EXPECT_FALSE(manager.IsAlive(e));
}

TEST_F(EntityManagerTest, Destroy_RecyclesEntityIndex) {
  SD::Entity e1 = manager.Create();
  uint32_t idx = e1.index;
  manager.Destroy(e1);

  SD::Entity e2 = manager.Create();
  EXPECT_EQ(e2.index, idx);
  EXPECT_NE(e2.generation, e1.generation);
}

TEST_F(EntityManagerTest, Destroy_IncrementsGeneration) {
  SD::Entity e1 = manager.Create();
  uint32_t gen = e1.generation;
  manager.Destroy(e1);

  SD::Entity e2 = manager.Create();
  EXPECT_EQ(e2.generation, gen + 1);
}

TEST_F(EntityManagerTest, AddComponent_GetComponent_ReturnsData) {
  SD::Entity e = manager.Create();
  manager.AddComponent<SD::Velocity>(e, 5.0f, 10.0f, 15.0f);

  SD::Velocity* v = manager.TryGetComponent<SD::Velocity>(e);
  ASSERT_NE(v, nullptr);
  EXPECT_FLOAT_EQ(v->x, 5.0f);
  EXPECT_FLOAT_EQ(v->y, 10.0f);
  EXPECT_FLOAT_EQ(v->z, 15.0f);
}

TEST_F(EntityManagerTest, HasComponent_ReturnsCorrectStatus) {
  SD::Entity e = manager.Create();

  EXPECT_FALSE(manager.HasComponent<SD::Velocity>(e));
  manager.AddComponent<SD::Velocity>(e, 1.0f, 2.0f, 3.0f);
  EXPECT_TRUE(manager.HasComponent<SD::Velocity>(e));
}

TEST_F(EntityManagerTest, TryRemoveComponent_RemovesComponent) {
  SD::Entity e = manager.Create();
  manager.AddComponent<SD::Velocity>(e, 1.0f, 2.0f, 3.0f);

  EXPECT_TRUE(manager.TryRemoveComponent<SD::Velocity>(e));
  EXPECT_FALSE(manager.HasComponent<SD::Velocity>(e));
  EXPECT_EQ(manager.TryGetComponent<SD::Velocity>(e), nullptr);
}

TEST_F(EntityManagerTest, TryRemoveComponent_NonExistent_ReturnsFalse) {
  SD::Entity e = manager.Create();
  EXPECT_FALSE(manager.TryRemoveComponent<SD::Velocity>(e));
}

TEST_F(EntityManagerTest, Destroy_RemovesAllComponents) {
  SD::Entity e = manager.Create();
  manager.AddComponent<SD::Velocity>(e, 1.0f, 2.0f, 3.0f);
  manager.AddComponent<SD::Health>(e, 100, 100);

  manager.Destroy(e);

  EXPECT_FALSE(manager.HasComponent<SD::Velocity>(e));
  EXPECT_FALSE(manager.HasComponent<SD::Health>(e));
}

TEST_F(EntityManagerTest, View_SingleComponent) {
  SD::Entity e1 = manager.Create();
  SD::Entity e2 = manager.Create();
  SD::Entity e3 = manager.Create();

  manager.AddComponent<SD::Velocity>(e1, 1.0f, 0.0f, 0.0f);
  manager.AddComponent<SD::Velocity>(e2, 2.0f, 0.0f, 0.0f);

  std::vector<SD::Entity> found;
  for (auto [entity, vel] : manager.View<SD::Velocity>()) {
    found.push_back(entity);
  }

  EXPECT_EQ(found.size(), 2u);
}

TEST_F(EntityManagerTest, View_MultipleComponents) {
  SD::Entity e1 = manager.Create();
  SD::Entity e2 = manager.Create();
  SD::Entity e3 = manager.Create();

  manager.AddComponent<SD::Velocity>(e1, 1.0f, 0.0f, 0.0f);
  manager.AddComponent<SD::Health>(e1, 100, 100);

  manager.AddComponent<SD::Velocity>(e2, 2.0f, 0.0f, 0.0f);

  manager.AddComponent<SD::Health>(e3, 50, 100);

  std::vector<SD::Entity> found;
  for (auto [entity, vel, health] : manager.View<SD::Velocity, SD::Health>()) {
    found.push_back(entity);
  }

  EXPECT_EQ(found.size(), 1u);
  EXPECT_EQ(found[0], e1);
}

TEST_F(EntityManagerTest, View_EmptyPool_ReturnsEmptyRange) {
  for (auto [entity, vel] : manager.View<SD::Velocity>()) {
    FAIL() << "View should be empty for non-existent component pool";
  }
}

TEST_F(EntityManagerTest, View_WithComponentGroup) {
  using RequiredComponents = SD::ComponentGroup<SD::Velocity, SD::Health>;

  SD::Entity e1 = manager.Create();
  manager.AddComponent<SD::Velocity>(e1, 1.0f, 0.0f, 0.0f);
  manager.AddComponent<SD::Health>(e1, 100, 100);

  SD::Entity e2 = manager.Create();
  manager.AddComponent<SD::Velocity>(e2, 2.0f, 0.0f, 0.0f);

  std::vector<SD::Entity> found;
  for (auto [entity, vel, health] : manager.View<RequiredComponents>()) {
    found.push_back(entity);
  }

  EXPECT_EQ(found.size(), 1u);
  EXPECT_EQ(found[0], e1);
}

TEST_F(EntityManagerTest, MultipleEntities_StressTest) {
  std::vector<SD::Entity> entities;
  for (int i = 0; i < 1000; ++i) {
    SD::Entity e = manager.Create();
    manager.AddComponent<SD::Velocity>(e, static_cast<float>(i), 0.0f, 0.0f);
    entities.push_back(e);
  }

  EXPECT_EQ(manager.GetEntityCount(), 1000);

  for (int i = 0; i < 1000; ++i) {
    SD::Velocity* v = manager.TryGetComponent<SD::Velocity>(entities[i]);
    ASSERT_NE(v, nullptr);
    EXPECT_FLOAT_EQ(v->x, static_cast<float>(i));
  }

  for (int i = 0; i < 500; ++i) {
    manager.Destroy(entities[i]);
  }

  int aliveCount = 0;
  for (int i = 0; i < 1000; ++i) {
    if (manager.IsAlive(entities[i])) {
      aliveCount++;
    }
  }
  EXPECT_EQ(aliveCount, 500);
}

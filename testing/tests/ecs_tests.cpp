#include "SD/core/ecs/Entity.hpp"
#include "SD/core/ecs/EntityManager.hpp"
#include "SD/core/ecs/component_registration.hpp"
#include "SD/core/logging.hpp"
#include "VLA/Matrix.hpp"
#include "gtest/gtest.h"
#include "spdlog/fmt/bundled/ostream.h"

namespace {
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
} // namespace
namespace sd {} // namespace sd

TEST(ECSTest, ReplaceComponent) {
  sd::EntityManager entityManager;
  sd::Entity        e = entityManager.create();

  VLA::Matrix4x4f initialMatrix = VLA::Matrix4x4f::Identity();
  entityManager.add_component<sd::Transform>(e, initialMatrix);

  sd::Transform* transPtrBefore = entityManager.try_get_component<sd::Transform>(e);
  ASSERT_NE(transPtrBefore, nullptr);
  EXPECT_EQ(transPtrBefore->transform, initialMatrix);

  VLA::Matrix4x4f newMatrix = VLA::Matrix4x4f::Translation({123.0f, 456.0f, 0.0f});
  entityManager.add_component<sd::Transform>(e, newMatrix);

  sd::Transform* transPtrAfter = entityManager.try_get_component<sd::Transform>(e);

  EXPECT_EQ(transPtrBefore, transPtrAfter)
      << "Pointer address changed! Was it a push_back instead of overwrite?";

  EXPECT_EQ(transPtrAfter->transform, newMatrix);

  auto components     = entityManager.get_all_component_info(e);
  int  transformCount = 0;
  for (const auto& cmp : components) {
    if (cmp.id == sd::ComponentTraits<sd::Transform>::id)
      transformCount++;
  }
  EXPECT_EQ(transformCount, 1) << "Entity reports multiple Transform components!";
}

class SparseEntitySetTest : public ::testing::Test {
protected:
  sd::SparseEntitySet<sd::Velocity> set;
};

TEST_F(SparseEntitySetTest, AddAndGet_ReturnsCorrectData) {
  sd::Entity e{0, 0};
  set.add(e, 1.0f, 2.0f, 3.0f);

  sd::Velocity* v = set.get(e);
  ASSERT_NE(v, nullptr);
  EXPECT_FLOAT_EQ(v->x, 1.0f);
  EXPECT_FLOAT_EQ(v->y, 2.0f);
  EXPECT_FLOAT_EQ(v->z, 3.0f);
}

TEST_F(SparseEntitySetTest, Get_NonExistent_ReturnsNullptr) {
  sd::Entity e{0, 0};
  EXPECT_EQ(set.get(e), nullptr);
}

TEST_F(SparseEntitySetTest, Remove_Existing_ReturnsTrue) {
  sd::Entity e{0, 0};
  set.add(e, 1.0f, 2.0f, 3.0f);

  EXPECT_TRUE(set.remove(e));
  EXPECT_EQ(set.get(e), nullptr);
  EXPECT_EQ(set.size(), 0u);
}

TEST_F(SparseEntitySetTest, Remove_NonExistent_ReturnsFalse) {
  sd::Entity e{0, 0};
  EXPECT_FALSE(set.remove(e));
}

TEST_F(SparseEntitySetTest, Remove_WrongGeneration_ReturnsFalse) {
  sd::Entity e1{0, 0};
  set.add(e1, 1.0f, 2.0f, 3.0f);

  sd::Entity e2{0, 1};
  EXPECT_FALSE(set.remove(e2));
  EXPECT_NE(set.get(e1), nullptr);
}

TEST_F(SparseEntitySetTest, Remove_SwapCorrectness) {
  sd::Entity e0{0, 0};
  sd::Entity e1{1, 0};
  sd::Entity e2{2, 0};

  set.add(e0, 10.0f, 0.0f, 0.0f);
  set.add(e1, 20.0f, 0.0f, 0.0f);
  set.add(e2, 30.0f, 0.0f, 0.0f);

  ASSERT_EQ(set.size(), 3u);

  EXPECT_TRUE(set.remove(e0));

  EXPECT_EQ(set.size(), 2u);
  EXPECT_EQ(set.get(e0), nullptr);

  sd::Velocity* v1 = set.get(e1);
  sd::Velocity* v2 = set.get(e2);

  ASSERT_NE(v1, nullptr);
  ASSERT_NE(v2, nullptr);
  EXPECT_FLOAT_EQ(v1->x, 20.0f);
  EXPECT_FLOAT_EQ(v2->x, 30.0f);
}

TEST_F(SparseEntitySetTest, Remove_MiddleElement_MaintainsIntegrity) {
  sd::Entity entities[5] = {
      {0, 0},
      {1, 0},
      {2, 0},
      {3, 0},
      {4, 0}
  };

  for (int i = 0; i < 5; ++i) {
    set.add(entities[i], static_cast<float>(i * 10), 0.0f, 0.0f);
  }

  EXPECT_TRUE(set.remove(entities[2]));

  for (int i = 0; i < 5; ++i) {
    sd::Velocity* v = set.get(entities[i]);
    if (i == 2) {
      EXPECT_EQ(v, nullptr);
    } else {
      ASSERT_NE(v, nullptr) << "Entity " << i << " should still exist";
      EXPECT_FLOAT_EQ(v->x, static_cast<float>(i * 10));
    }
  }
}

TEST_F(SparseEntitySetTest, SerializeDeserialize_RoundTrip) {
  sd::Entity e0{0, 0};
  sd::Entity e1{100, 0};
  sd::Entity e2{2048, 0};

  set.add(e0, 1.0f, 2.0f, 3.0f);
  set.add(e1, 10.0f, 20.0f, 30.0f);
  set.add(e2, 100.0f, 200.0f, 300.0f);

  std::vector<char> buffer;
  set.serialize_to(buffer);

  sd::SparseEntitySet<sd::Velocity> deserialized;
  deserialized.deserialize_from(buffer);

  EXPECT_EQ(deserialized.size(), 3u);

  sd::Velocity* v0 = deserialized.get(e0);
  sd::Velocity* v1 = deserialized.get(e1);
  sd::Velocity* v2 = deserialized.get(e2);

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
  set.serialize_to(buffer);

  sd::SparseEntitySet<sd::Velocity> deserialized;
  deserialized.deserialize_from(buffer);

  EXPECT_EQ(deserialized.size(), 0u);
}

class EntityManagerTest : public ::testing::Test {
protected:
  sd::EntityManager manager;
};

TEST_F(EntityManagerTest, Create_ReturnsAliveEntity) {
  sd::Entity e = manager.create();
  EXPECT_TRUE(manager.is_alive(e));
}

TEST_F(EntityManagerTest, Destroy_EntityNoLongerAlive) {
  sd::Entity e = manager.create();
  manager.destroy(e);
  EXPECT_FALSE(manager.is_alive(e));
}

TEST_F(EntityManagerTest, Destroy_RecyclesEntityIndex) {
  sd::Entity e1  = manager.create();
  uint32_t   idx = e1.index;
  manager.destroy(e1);

  sd::Entity e2 = manager.create();
  EXPECT_EQ(e2.index, idx);
  EXPECT_NE(e2.generation, e1.generation);
}

TEST_F(EntityManagerTest, Destroy_IncrementsGeneration) {
  sd::Entity e1  = manager.create();
  uint32_t   gen = e1.generation;
  manager.destroy(e1);

  sd::Entity e2 = manager.create();
  EXPECT_EQ(e2.generation, gen + 1);
}

TEST_F(EntityManagerTest, AddComponent_GetComponent_ReturnsData) {
  sd::Entity e = manager.create();
  manager.add_component<sd::Velocity>(e, 5.0f, 10.0f, 15.0f);

  sd::Velocity* v = manager.try_get_component<sd::Velocity>(e);
  ASSERT_NE(v, nullptr);
  EXPECT_FLOAT_EQ(v->x, 5.0f);
  EXPECT_FLOAT_EQ(v->y, 10.0f);
  EXPECT_FLOAT_EQ(v->z, 15.0f);
}

TEST_F(EntityManagerTest, HasComponent_ReturnsCorrectStatus) {
  sd::Entity e = manager.create();

  EXPECT_FALSE(manager.has_component<sd::Velocity>(e));
  manager.add_component<sd::Velocity>(e, 1.0f, 2.0f, 3.0f);
  EXPECT_TRUE(manager.has_component<sd::Velocity>(e));
}

TEST_F(EntityManagerTest, TryRemoveComponent_RemovesComponent) {
  sd::Entity e = manager.create();
  manager.add_component<sd::Velocity>(e, 1.0f, 2.0f, 3.0f);

  EXPECT_TRUE(manager.try_remove_component<sd::Velocity>(e));
  EXPECT_FALSE(manager.has_component<sd::Velocity>(e));
  EXPECT_EQ(manager.try_get_component<sd::Velocity>(e), nullptr);
}

TEST_F(EntityManagerTest, TryRemoveComponent_NonExistent_ReturnsFalse) {
  sd::Entity e = manager.create();
  EXPECT_FALSE(manager.try_remove_component<sd::Velocity>(e));
}

TEST_F(EntityManagerTest, Destroy_RemovesAllComponents) {
  sd::Entity e = manager.create();
  manager.add_component<sd::Velocity>(e, 1.0f, 2.0f, 3.0f);
  manager.add_component<sd::Health>(e, 100, 100);

  manager.destroy(e);

  EXPECT_FALSE(manager.has_component<sd::Velocity>(e));
  EXPECT_FALSE(manager.has_component<sd::Health>(e));
}

TEST_F(EntityManagerTest, View_SingleComponent) {
  sd::Entity e1 = manager.create();
  sd::Entity e2 = manager.create();
  (void)manager.create();

  manager.add_component<sd::Velocity>(e1, 1.0f, 0.0f, 0.0f);
  manager.add_component<sd::Velocity>(e2, 2.0f, 0.0f, 0.0f);

  std::vector<sd::Entity> found;
  for (auto [entity, vel] : manager.view<sd::Velocity>()) {
    found.push_back(entity);
  }

  EXPECT_EQ(found.size(), 2u);
}

TEST_F(EntityManagerTest, View_MultipleComponents) {
  sd::Entity e1 = manager.create();
  sd::Entity e2 = manager.create();
  sd::Entity e3 = manager.create();

  manager.add_component<sd::Velocity>(e1, 1.0f, 0.0f, 0.0f);
  manager.add_component<sd::Health>(e1, 100, 100);

  manager.add_component<sd::Velocity>(e2, 2.0f, 0.0f, 0.0f);

  manager.add_component<sd::Health>(e3, 50, 100);

  std::vector<sd::Entity> found;
  for (auto [entity, vel, health] : manager.view<sd::Velocity, sd::Health>()) {
    found.push_back(entity);
  }

  EXPECT_EQ(found.size(), 1u);
  EXPECT_EQ(found[0], e1);
}

TEST_F(EntityManagerTest, View_EmptyPool_ReturnsEmptyRange) {
  for ([[maybe_unused]] auto [entity, vel] : manager.view<sd::Velocity>()) {
    FAIL() << "View should be empty for non-existent component pool";
  }
}

TEST_F(EntityManagerTest, View_WithComponentGroup) {
  using RequiredComponents = sd::ComponentGroup<sd::Velocity, sd::Health>;

  sd::Entity e1 = manager.create();
  manager.add_component<sd::Velocity>(e1, 1.0f, 0.0f, 0.0f);
  manager.add_component<sd::Health>(e1, 100, 100);

  sd::Entity e2 = manager.create();
  manager.add_component<sd::Velocity>(e2, 2.0f, 0.0f, 0.0f);

  std::vector<sd::Entity> found;
  for (auto [entity, vel, health] : manager.view<RequiredComponents>()) {
    found.push_back(entity);
  }

  EXPECT_EQ(found.size(), 1u);
  EXPECT_EQ(found[0], e1);
}

TEST_F(EntityManagerTest, MultipleEntities_StressTest) {
  std::vector<sd::Entity> entities;
  for (int i = 0; i < 1000; ++i) {
    sd::Entity e = manager.create();
    manager.add_component<sd::Velocity>(e, static_cast<float>(i), 0.0f, 0.0f);
    entities.push_back(e);
  }

  EXPECT_EQ(manager.get_entity_count(), 1000);

  for (int i = 0; i < 1000; ++i) {
    sd::Velocity* v = manager.try_get_component<sd::Velocity>(entities[i]);
    ASSERT_NE(v, nullptr);
    EXPECT_FLOAT_EQ(v->x, static_cast<float>(i));
  }

  for (int i = 0; i < 500; ++i) {
    manager.destroy(entities[i]);
  }

  int aliveCount = 0;
  for (int i = 0; i < 1000; ++i) {
    if (manager.is_alive(entities[i])) {
      aliveCount++;
    }
  }
  EXPECT_EQ(aliveCount, 500);
}

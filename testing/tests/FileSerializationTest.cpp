#include <gtest/gtest.h>

#include "SD/core/ecs/CommandQueue.hpp"
#include "SD/core/ecs/commands.hpp"
#include "SD/core/ecs/components.hpp"
#include "SD/core/ecs/EntityManager.hpp"
#include "utils/file_utils.hpp"
#include "utils/serialization.hpp"

namespace sd {

class FileSerializationTest : public ::testing::Test {
protected:
  void TearDown() override {
    std::remove("test_commandqueue.bin");
    std::remove("test_ecs.bin");
    std::remove("test_ecs2.bin");
  }
};

TEST_F(FileSerializationTest, Filewriting) {
  constexpr double d{3.14};
  std::vector<std::byte> buffer;

  Serializer(buffer).write(d);
  Filesystem::write_binary("test.bin", buffer);

  std::vector<std::byte> read_buffer;
  Filesystem::read_binary("test.bin", read_buffer);
  const double d2 = Serializer(read_buffer).read<double>();

  EXPECT_EQ(d, d2);
}

TEST_F(FileSerializationTest, CommandQueue_FileRoundTrip) {
  CommandQueue queue;
  EntityHandle h(0);
  queue.add<CreateEntityCmd>(h);
  queue.add<AddComponentCmd<Transform>>(h, Transform{VLA::Matrix4x4f::Identity()});

  std::vector<std::byte> buffer;
  Serializer serializer(buffer);
  queue.serialize(serializer);
  Filesystem::write_binary("test_commandqueue.bin", buffer);

  std::vector<std::byte> read_buffer;
  Filesystem::read_binary("test_commandqueue.bin", read_buffer);

  CommandQueue queue2;
  Serializer deserializer(read_buffer);
  deserializer.reset_offset();
  queue2.deserialize(deserializer);

  EXPECT_EQ(queue.get_count(), queue2.get_count());
}

TEST_F(FileSerializationTest, CommandQueue_FileWithMultipleEntities) {
  CommandQueue queue;
  EntityHandle h1(0);
  EntityHandle h2(1);
  queue.add<CreateEntityCmd>(h1);
  queue.add<CreateEntityCmd>(h2);
  queue.add<AddComponentCmd<Transform>>(h1, Transform{VLA::Matrix4x4f::Identity()});
  queue.add<AddComponentCmd<Transform>>(h2, Transform{VLA::Matrix4x4f::Identity() * 2.0f});

  std::vector<std::byte> buffer;
  Serializer serializer(buffer);
  queue.serialize(serializer);
  Filesystem::write_binary("test_commandqueue.bin", buffer);

  std::vector<std::byte> read_buffer;
  Filesystem::read_binary("test_commandqueue.bin", read_buffer);

  CommandQueue queue2;
  Serializer deserializer(read_buffer);
  deserializer.reset_offset();
  queue2.deserialize(deserializer);

  EXPECT_EQ(queue2.get_count(), 4u);
}

TEST_F(FileSerializationTest, ECS_EntityManagerSerialize) {
  EntityManager em;
  Entity e1 = em.create();
  Entity e2 = em.create();
  em.add_component<Transform>(e1, VLA::Matrix4x4f::Identity());
  em.add_component<Transform>(e2, VLA::Matrix4x4f::Identity() * 2.0f);

  std::vector<std::byte> buffer;
  Serializer serializer(buffer);
  em.serialize(serializer);
  Filesystem::write_binary("test_ecs.bin", buffer);

  std::vector<std::byte> readBuffer;
  Filesystem::read_binary("test_ecs.bin", readBuffer);

  EntityManager em2;
  Serializer deserializer(readBuffer);
  deserializer.reset_offset();
  em2.deserialize(deserializer);

  EXPECT_EQ(em.get_entity_count(), em2.get_entity_count());
  EXPECT_TRUE(em2.is_alive(e1));
  EXPECT_TRUE(em2.is_alive(e2));
  EXPECT_TRUE(em2.has_component<Transform>(e1));
  EXPECT_TRUE(em2.has_component<Transform>(e2));
}

TEST_F(FileSerializationTest, ECS_EmptyEntityManager) {
  EntityManager em;

  std::vector<std::byte> buffer;
  Serializer serializer(buffer);
  em.serialize(serializer);
  Filesystem::write_binary("test_ecs.bin", buffer);

  std::vector<std::byte> readBuffer;
  Filesystem::read_binary("test_ecs.bin", readBuffer);

  EntityManager em2;
  Serializer deserializer(readBuffer);
  deserializer.reset_offset();
  em2.deserialize(deserializer);

  EXPECT_EQ(em2.get_entity_count(), 0u);
}

TEST_F(FileSerializationTest, ECS_MultipleComponentsSameEntity) {
  EntityManager em;
  Entity e1 = em.create();
  em.add_component<Transform>(e1, VLA::Matrix4x4f::Identity());
  em.add_component<DebugName>(e1, "EntityOne");

  std::vector<std::byte> buffer;
  Serializer serializer(buffer);
  em.serialize(serializer);
  Filesystem::write_binary("test_ecs.bin", buffer);

  std::vector<std::byte> readBuffer;
  Filesystem::read_binary("test_ecs.bin", readBuffer);

  EntityManager em2;
  Serializer deserializer(readBuffer);
  deserializer.reset_offset();
  em2.deserialize(deserializer);

  EXPECT_EQ(em2.get_entity_count(), 1u);
  EXPECT_TRUE(em2.has_component<Transform>(e1));
  EXPECT_TRUE(em2.has_component<DebugName>(e1));
  const DebugName* name = em2.try_get_component<DebugName>(e1);
  EXPECT_STREQ(name->name.c_str(), "EntityOne");
}

TEST_F(FileSerializationTest, ECS_VerifyComponentData) {
  EntityManager em;
  Entity e1 = em.create();
  Entity e2 = em.create();
  em.add_component<Transform>(e1, VLA::Matrix4x4f::Identity() * 3.0f);
  em.add_component<Transform>(e2, VLA::Matrix4x4f::Identity() * 5.0f);
  em.add_component<DebugName>(e1, "First");
  em.add_component<DebugName>(e2, "Second");

  std::vector<std::byte> buffer;
  Serializer serializer(buffer);
  em.serialize(serializer);
  Filesystem::write_binary("test_ecs.bin", buffer);

  std::vector<std::byte> readBuffer;
  Filesystem::read_binary("test_ecs.bin", readBuffer);

  EntityManager em2;
  Serializer deserializer(readBuffer);
  deserializer.reset_offset();
  em2.deserialize(deserializer);

  const Transform* t1 = em2.try_get_component<Transform>(e1);
  const Transform* t2 = em2.try_get_component<Transform>(e2);
  const DebugName* n1 = em2.try_get_component<DebugName>(e1);
  const DebugName* n2 = em2.try_get_component<DebugName>(e2);

  ASSERT_NE(t1, nullptr);
  ASSERT_NE(t2, nullptr);
  ASSERT_NE(n1, nullptr);
  ASSERT_NE(n2, nullptr);

  EXPECT_FLOAT_EQ(t1->world_matrix.A[0], 3.0f);
  EXPECT_FLOAT_EQ(t2->world_matrix.A[0], 5.0f);
  EXPECT_STREQ(n1->name.c_str(), "First");
  EXPECT_STREQ(n2->name.c_str(), "Second");
}

TEST_F(FileSerializationTest, ECS_DestroyedEntityNotSerialized) {
  EntityManager em;
  Entity e1 = em.create();
  Entity e2 = em.create();
  em.add_component<Transform>(e1, VLA::Matrix4x4f::Identity());
  em.add_component<Transform>(e2, VLA::Matrix4x4f::Identity());
  em.destroy(e1);

  std::vector<std::byte> buffer;
  Serializer serializer(buffer);
  em.serialize(serializer);
  Filesystem::write_binary("test_ecs.bin", buffer);

  std::vector<std::byte> readBuffer;
  Filesystem::read_binary("test_ecs.bin", readBuffer);

  EntityManager em2;
  Serializer deserializer(readBuffer);
  deserializer.reset_offset();
  em2.deserialize(deserializer);

  (void)e2;

  EXPECT_EQ(em2.get_alive_entity_count(), 1u);
  EXPECT_FALSE(em2.is_alive(e1));
  EXPECT_TRUE(em2.is_alive(e2));
  EXPECT_NE(nullptr, em2.try_get_component<Transform>(e2));
}

TEST_F(FileSerializationTest, ECS_RemoveComponentSerialized) {
  EntityManager em;
  Entity e1 = em.create();
  em.add_component<Transform>(e1, VLA::Matrix4x4f::Identity());
  em.add_component<DebugName>(e1, "Test");
  em.try_remove_component<DebugName>(e1);

  std::vector<std::byte> buffer;
  Serializer serializer(buffer);
  em.serialize(serializer);
  Filesystem::write_binary("test_ecs.bin", buffer);

  std::vector<std::byte> readBuffer;
  Filesystem::read_binary("test_ecs.bin", readBuffer);

  EntityManager em2;
  Serializer deserializer(readBuffer);
  deserializer.reset_offset();
  em2.deserialize(deserializer);

  EXPECT_TRUE(em2.has_component<Transform>(e1));
  EXPECT_FALSE(em2.has_component<DebugName>(e1));
}

TEST_F(FileSerializationTest, ECS_ManyEntities) {
  constexpr int ENTITY_COUNT = 100;
  EntityManager em;
  std::vector<Entity> entities;
  entities.reserve(ENTITY_COUNT);

  for (int i = 0; i < ENTITY_COUNT; ++i) {
    Entity e = em.create();
    entities.push_back(e);
    em.add_component<Transform>(e, VLA::Matrix4x4f::Identity() * static_cast<float>(i + 1));
    em.add_component<DebugName>(e, "Entity" + std::to_string(i));
  }

  std::vector<std::byte> buffer;
  Serializer serializer(buffer);
  em.serialize(serializer);
  Filesystem::write_binary("test_ecs.bin", buffer);

  std::vector<std::byte> readBuffer;
  Filesystem::read_binary("test_ecs.bin", readBuffer);

  EntityManager em2;
  Serializer deserializer(readBuffer);
  deserializer.reset_offset();
  em2.deserialize(deserializer);

  EXPECT_EQ(em2.get_entity_count(), ENTITY_COUNT);

  for (int i = 0; i < ENTITY_COUNT; ++i) {
    Entity e = entities[i];
    EXPECT_TRUE(em2.is_alive(e));
    EXPECT_TRUE(em2.has_component<Transform>(e));
    EXPECT_TRUE(em2.has_component<DebugName>(e));

    const Transform* t = em2.try_get_component<Transform>(e);
    EXPECT_FLOAT_EQ(t->world_matrix.A[0], static_cast<float>(i + 1));

    const DebugName* n = em2.try_get_component<DebugName>(e);
    EXPECT_STREQ(n->name.c_str(), ("Entity" + std::to_string(i)).c_str());
  }
}

TEST_F(FileSerializationTest, ECS_DoubleSerializationConsistent) {
  EntityManager em;
  Entity e1 = em.create();
  em.add_component<Transform>(e1, VLA::Matrix4x4f::Identity());
  em.add_component<DebugName>(e1, "Test");

  std::vector<std::byte> buffer1;
  Serializer ser1(buffer1);
  em.serialize(ser1);

  std::vector<std::byte> buffer2;
  Serializer ser2(buffer2);
  em.serialize(ser2);

  EXPECT_EQ(buffer1.size(), buffer2.size());
  EXPECT_EQ(buffer1, buffer2);
}

TEST_F(FileSerializationTest, ECS_DeserializeIntoNonEmpty) {
  EntityManager em;
  Entity existing = em.create();
  em.add_component<Transform>(existing, VLA::Matrix4x4f::Identity() * 10.0f);

  std::vector<std::byte> buffer;
  Serializer serializer(buffer);
  em.serialize(serializer);

  EntityManager em2;
  Entity e3 = em2.create();
  em2.add_component<Transform>(e3, VLA::Matrix4x4f::Identity() * 99.0f);

  Serializer deserializer(buffer);
  deserializer.reset_offset();
  em2.deserialize(deserializer);

  EXPECT_EQ(em2.get_entity_count(), 1u);
  EXPECT_TRUE(em2.is_alive(existing));

  const Transform* tExisting = em2.try_get_component<Transform>(existing);
  ASSERT_NE(tExisting, nullptr);
  EXPECT_FLOAT_EQ(tExisting->world_matrix.A[0], 10.0f);
}

TEST_F(FileSerializationTest, ECS_GenerationAfterDeserialize) {
  EntityManager em;
  Entity e1 = em.create();
  em.add_component<Transform>(e1, VLA::Matrix4x4f::Identity());
  em.destroy(e1);
  Entity e2 = em.create();
  em.add_component<Transform>(e2, VLA::Matrix4x4f::Identity() * 2.0f);

  std::vector<std::byte> buffer;
  Serializer serializer(buffer);
  em.serialize(serializer);

  EntityManager em2;
  Serializer deserializer(buffer);
  deserializer.reset_offset();
  em2.deserialize(deserializer);

  EXPECT_EQ(em2.get_entity_count(), 1u);
  EXPECT_TRUE(em2.is_alive(e2));

  Entity e3 = em2.create();
  EXPECT_NE(e3.index, e2.index);

  em2.add_component<DebugName>(e3, "newlyCreated");
  EXPECT_TRUE(em2.is_alive(e3));
  EXPECT_NE(nullptr, em2.try_get_component<DebugName>(e3));
}

TEST_F(FileSerializationTest, ECS_FreeListPreserved) {
  EntityManager em;
  std::vector<Entity> created;
  for (int i = 0; i < 10; ++i) {
    created.push_back(em.create());
  }
  for (int i = 0; i < 5; ++i) {
    em.destroy(created[i]);
  }

  std::vector<std::byte> buffer;
  Serializer serializer(buffer);
  em.serialize(serializer);

  EntityManager em2;
  Serializer deserializer(buffer);
  deserializer.reset_offset();
  em2.deserialize(deserializer);

  EXPECT_EQ(em2.get_alive_entity_count(), 5u);

  Entity e = em2.create();
  EXPECT_TRUE(em2.is_alive(e));
  em2.add_component<Transform>(e, VLA::Matrix4x4f::Identity());
  EXPECT_NE(nullptr, em2.try_get_component<Transform>(e));
}

TEST_F(FileSerializationTest, ECS_EmptyString) {
  EntityManager em;
  Entity e1 = em.create();
  em.add_component<DebugName>(e1, "");
  em.add_component<Transform>(e1, VLA::Matrix4x4f::Identity());

  std::vector<std::byte> buffer;
  Serializer serializer(buffer);
  em.serialize(serializer);
  Filesystem::write_binary("test_ecs.bin", buffer);

  std::vector<std::byte> readBuffer;
  Filesystem::read_binary("test_ecs.bin", readBuffer);

  EntityManager em2;
  Serializer deserializer(readBuffer);
  deserializer.reset_offset();
  em2.deserialize(deserializer);

  const DebugName* n = em2.try_get_component<DebugName>(e1);
  ASSERT_NE(n, nullptr);
  EXPECT_EQ(n->name, "");
}

TEST_F(FileSerializationTest, ECS_LongString) {
  std::string longStr(1000, 'X');
  longStr += "END";

  EntityManager em;
  Entity e1 = em.create();
  em.add_component<DebugName>(e1, longStr);

  std::vector<std::byte> buffer;
  Serializer serializer(buffer);
  em.serialize(serializer);

  EntityManager em2;
  Serializer deserializer(buffer);
  deserializer.reset_offset();
  em2.deserialize(deserializer);

  const DebugName* n = em2.try_get_component<DebugName>(e1);
  ASSERT_NE(n, nullptr);
  EXPECT_EQ(n->name, longStr);
}

TEST_F(FileSerializationTest, ECS_DoubleRoundTrip) {
  EntityManager em;
  Entity e1 = em.create();
  em.add_component<Transform>(e1, VLA::Matrix4x4f::Identity());
  em.add_component<DebugName>(e1, "Test");

  std::vector<std::byte> buffer1;
  Serializer ser1(buffer1);
  em.serialize(ser1);

  EntityManager em2;
  Serializer des1(buffer1);
  des1.reset_offset();
  em2.deserialize(des1);

  std::vector<std::byte> buffer2;
  Serializer ser2(buffer2);
  em2.serialize(ser2);

  EXPECT_EQ(buffer1.size(), buffer2.size());
  EXPECT_EQ(buffer1, buffer2);

  EntityManager em3;
  Serializer des2(buffer2);
  des2.reset_offset();
  em3.deserialize(des2);

  EXPECT_EQ(em3.get_entity_count(), 1u);
  EXPECT_TRUE(em3.is_alive(e1));
  const Transform* t = em3.try_get_component<Transform>(e1);
  const DebugName* n = em3.try_get_component<DebugName>(e1);
  ASSERT_NE(t, nullptr);
  ASSERT_NE(n, nullptr);
  EXPECT_FLOAT_EQ(t->world_matrix.A[0], 1.0f);
  EXPECT_EQ(n->name, "Test");
}

TEST_F(FileSerializationTest, ECS_AllComponentTypes) {
  EntityManager em;
  Entity e1 = em.create();
  em.add_component<Transform>(e1, VLA::Matrix4x4f::Identity());
  em.add_component<DebugName>(e1, "TestEntity");

  Entity e2 = em.create();
  em.add_component<Renderable>(e2);
  em.get_component<Renderable>(e2).mesh_id = 42;
  em.get_component<Renderable>(e2).material_id = 99;
  em.get_component<Renderable>(e2).color[0] = 0.5f;

  std::vector<std::byte> buffer;
  Serializer serializer(buffer);
  em.serialize(serializer);

  EntityManager em2;
  Serializer deserializer(buffer);
  deserializer.reset_offset();
  em2.deserialize(deserializer);

  EXPECT_EQ(em2.get_entity_count(), 2u);

  const Transform* t = em2.try_get_component<Transform>(e1);
  const DebugName* n = em2.try_get_component<DebugName>(e1);
  const Renderable* r = em2.try_get_component<Renderable>(e2);

  ASSERT_NE(t, nullptr);
  ASSERT_NE(n, nullptr);
  ASSERT_NE(r, nullptr);

  EXPECT_FLOAT_EQ(t->world_matrix.A[0], 1.0f);
  EXPECT_EQ(n->name, "TestEntity");
  EXPECT_EQ(r->mesh_id, 42u);
  EXPECT_EQ(r->material_id, 99u);
  EXPECT_FLOAT_EQ(r->color[0], 0.5f);
}

TEST_F(FileSerializationTest, ECS_SerializableComponentsOnly) {
  EntityManager em;
  Entity e1 = em.create();
  em.add_component<Transform>(e1, VLA::Matrix4x4f::Identity());
  em.add_component<DebugName>(e1, "Test");

  std::vector<std::byte> buffer;
  Serializer serializer(buffer);
  em.serialize(serializer);

  EntityManager em2;
  Serializer deserializer(buffer);
  deserializer.reset_offset();
  em2.deserialize(deserializer);

  auto* transformPool = em2.get_component_pool<Transform>();
  auto* debugNamePool = em2.get_component_pool<DebugName>();
  auto* cameraPool = em2.get_component_pool<Camera>();

  EXPECT_NE(transformPool, nullptr);
  EXPECT_NE(debugNamePool, nullptr);
  EXPECT_EQ(cameraPool, nullptr);
}

TEST_F(FileSerializationTest, ECS_IndependentInstances) {
  EntityManager em1;
  Entity e1 = em1.create();
  em1.add_component<Transform>(e1, VLA::Matrix4x4f::Identity());
  em1.add_component<DebugName>(e1, "FirstECS");

  EntityManager em2;
  Entity e2 = em2.create();
  em2.add_component<Transform>(e2, VLA::Matrix4x4f::Identity() * 5.0f);
  em2.add_component<DebugName>(e2, "SecondECS");

  std::vector<std::byte> buffer1, buffer2;
  Serializer ser1(buffer1);
  Serializer ser2(buffer2);
  em1.serialize(ser1);
  em2.serialize(ser2);

  EntityManager em1Restored;
  EntityManager em2Restored;

  Serializer des1(buffer1);
  des1.reset_offset();
  em1Restored.deserialize(des1);

  Serializer des2(buffer2);
  des2.reset_offset();
  em2Restored.deserialize(des2);

  EXPECT_EQ(em1Restored.get_entity_count(), 1u);
  EXPECT_EQ(em2Restored.get_entity_count(), 1u);

  const Transform* t1 = em1Restored.try_get_component<Transform>(e1);
  const Transform* t2 = em2Restored.try_get_component<Transform>(e2);
  const DebugName* n1 = em1Restored.try_get_component<DebugName>(e1);
  const DebugName* n2 = em2Restored.try_get_component<DebugName>(e2);

  ASSERT_NE(t1, nullptr);
  ASSERT_NE(t2, nullptr);
  ASSERT_NE(n1, nullptr);
  ASSERT_NE(n2, nullptr);

  EXPECT_FLOAT_EQ(t1->world_matrix.A[0], 1.0f);
  EXPECT_FLOAT_EQ(t2->world_matrix.A[0], 5.0f);
  EXPECT_EQ(n1->name, "FirstECS");
  EXPECT_EQ(n2->name, "SecondECS");

  EXPECT_TRUE(em1Restored.is_alive(e1));
  EXPECT_TRUE(em2Restored.is_alive(e2));

  Entity newInEM1 = em1Restored.create();
  Entity newInEM2 = em2Restored.create();

  EXPECT_EQ(em1Restored.get_entity_count(), 2u);
  EXPECT_EQ(em2Restored.get_entity_count(), 2u);

  em1Restored.add_component<Transform>(newInEM1, VLA::Matrix4x4f::Identity() * 10.0f);
  em2Restored.add_component<Transform>(newInEM2, VLA::Matrix4x4f::Identity() * 20.0f);

  const Transform* tNew1 = em1Restored.try_get_component<Transform>(newInEM1);
  const Transform* tNew2 = em2Restored.try_get_component<Transform>(newInEM2);
  ASSERT_NE(tNew1, nullptr);
  ASSERT_NE(tNew2, nullptr);
  EXPECT_FLOAT_EQ(tNew1->world_matrix.A[0], 10.0f);
  EXPECT_FLOAT_EQ(tNew2->world_matrix.A[0], 20.0f);

  em1Restored.destroy(e1);
  EXPECT_EQ(em1Restored.get_entity_count(), 2u);
  EXPECT_FALSE(em1Restored.is_alive(e1));
  EXPECT_EQ(em2Restored.get_entity_count(), 2u);
  EXPECT_TRUE(em2Restored.is_alive(e2));
}

TEST_F(FileSerializationTest, ECS_DeserializeFirstThenCreate) {
  std::vector<std::byte> buffer;
  {
    EntityManager em;
    Entity e = em.create();
    em.add_component<Transform>(e, VLA::Matrix4x4f::Identity());

    Serializer ser(buffer);
    em.serialize(ser);
  }

  EntityManager em;
  Serializer des(buffer);
  des.reset_offset();
  em.deserialize(des);

  EXPECT_EQ(em.get_entity_count(), 1u);

  Entity newEntity = em.create();
  em.add_component<Transform>(newEntity, VLA::Matrix4x4f::Identity() * 2.0f);
  em.add_component<DebugName>(newEntity, "CreatedAfter");

  EXPECT_EQ(em.get_entity_count(), 2u);
  EXPECT_TRUE(em.is_alive(newEntity));

  const Transform* t = em.try_get_component<Transform>(newEntity);
  const DebugName* n = em.try_get_component<DebugName>(newEntity);
  ASSERT_NE(t, nullptr);
  ASSERT_NE(n, nullptr);
  EXPECT_FLOAT_EQ(t->world_matrix.A[0], 2.0f);
  EXPECT_EQ(n->name, "CreatedAfter");
}

TEST_F(FileSerializationTest, ECS_IdempotentSerialization) {
  EntityManager em;
  Entity e1 = em.create();
  em.add_component<Transform>(e1, VLA::Matrix4x4f::Identity());
  em.add_component<DebugName>(e1, "Test");

  std::vector<std::byte> b1, b2, b3;
  Serializer s1(b1), s2(b2), s3(b3);
  em.serialize(s1);
  em.serialize(s2);
  em.serialize(s3);

  EXPECT_EQ(b1, b2);
  EXPECT_EQ(b2, b3);
}

} // namespace sd

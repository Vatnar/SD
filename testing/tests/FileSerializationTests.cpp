#include <fstream>
#include <gtest/gtest.h>

#include "Core/ECS/CommandQueue.hpp"
#include "Core/ECS/Commands.hpp"
#include "Core/ECS/Components.hpp"
#include "Core/ECS/EntityManager.hpp"
#include "Utils/FileUtils.hpp"
#include "Utils/Serialization.hpp"

namespace SD {

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

  Serializer(buffer).Write(d);
  Filesystem::WriteBinary("test.bin", buffer);

  std::vector<std::byte> readBuffer;
  Filesystem::ReadBinary("test.bin", readBuffer);
  const double d2 = Serializer(readBuffer).Read<double>();

  EXPECT_EQ(d, d2);
}

TEST_F(FileSerializationTest, CommandQueue_FileRoundTrip) {
  CommandQueue queue;
  EntityHandle h(0);
  queue.Add<CreateEntityCmd>(h);
  queue.Add<AddComponentCmd<Transform>>(h, Transform{VLA::Matrix4x4f::Identity});

  std::vector<std::byte> buffer;
  Serializer serializer(buffer);
  queue.Serialize(serializer);
  Filesystem::WriteBinary("test_commandqueue.bin", buffer);

  std::vector<std::byte> readBuffer;
  Filesystem::ReadBinary("test_commandqueue.bin", readBuffer);

  CommandQueue queue2;
  Serializer deserializer(readBuffer);
  deserializer.ResetOffset();
  queue2.Deserialize(deserializer);

  EXPECT_EQ(queue.GetCount(), queue2.GetCount());
}

TEST_F(FileSerializationTest, CommandQueue_FileWithMultipleEntities) {
  CommandQueue queue;
  EntityHandle h1(0);
  EntityHandle h2(1);
  queue.Add<CreateEntityCmd>(h1);
  queue.Add<CreateEntityCmd>(h2);
  queue.Add<AddComponentCmd<Transform>>(h1, Transform{VLA::Matrix4x4f::Identity});
  queue.Add<AddComponentCmd<Transform>>(h2, Transform{VLA::Matrix4x4f::Identity * 2.0f});

  std::vector<std::byte> buffer;
  Serializer serializer(buffer);
  queue.Serialize(serializer);
  Filesystem::WriteBinary("test_commandqueue.bin", buffer);

  std::vector<std::byte> readBuffer;
  Filesystem::ReadBinary("test_commandqueue.bin", readBuffer);

  CommandQueue queue2;
  Serializer deserializer(readBuffer);
  deserializer.ResetOffset();
  queue2.Deserialize(deserializer);

  EXPECT_EQ(queue2.GetCount(), 4u);
}

TEST_F(FileSerializationTest, ECS_EntityManagerSerialize) {
  EntityManager em;
  Entity e1 = em.Create();
  Entity e2 = em.Create();
  em.AddComponent<Transform>(e1, VLA::Matrix4x4f::Identity);
  em.AddComponent<Transform>(e2, VLA::Matrix4x4f::Identity * 2.0f);

  std::vector<std::byte> buffer;
  Serializer serializer(buffer);
  em.Serialize(serializer);
  Filesystem::WriteBinary("test_ecs.bin", buffer);

  std::vector<std::byte> readBuffer;
  Filesystem::ReadBinary("test_ecs.bin", readBuffer);

  EntityManager em2;
  Serializer deserializer(readBuffer);
  deserializer.ResetOffset();
  em2.Deserialize(deserializer);

  EXPECT_EQ(em.GetEntityCount(), em2.GetEntityCount());
  EXPECT_TRUE(em2.IsAlive(e1));
  EXPECT_TRUE(em2.IsAlive(e2));
  EXPECT_TRUE(em2.HasComponent<Transform>(e1));
  EXPECT_TRUE(em2.HasComponent<Transform>(e2));
}

TEST_F(FileSerializationTest, ECS_EmptyEntityManager) {
  EntityManager em;

  std::vector<std::byte> buffer;
  Serializer serializer(buffer);
  em.Serialize(serializer);
  Filesystem::WriteBinary("test_ecs.bin", buffer);

  std::vector<std::byte> readBuffer;
  Filesystem::ReadBinary("test_ecs.bin", readBuffer);

  EntityManager em2;
  Serializer deserializer(readBuffer);
  deserializer.ResetOffset();
  em2.Deserialize(deserializer);

  EXPECT_EQ(em2.GetEntityCount(), 0u);
}

TEST_F(FileSerializationTest, ECS_MultipleComponentsSameEntity) {
  EntityManager em;
  Entity e1 = em.Create();
  em.AddComponent<Transform>(e1, VLA::Matrix4x4f::Identity);
  em.AddComponent<DebugName>(e1, "EntityOne");

  std::vector<std::byte> buffer;
  Serializer serializer(buffer);
  em.Serialize(serializer);
  Filesystem::WriteBinary("test_ecs.bin", buffer);

  std::vector<std::byte> readBuffer;
  Filesystem::ReadBinary("test_ecs.bin", readBuffer);

  EntityManager em2;
  Serializer deserializer(readBuffer);
  deserializer.ResetOffset();
  em2.Deserialize(deserializer);

  EXPECT_EQ(em2.GetEntityCount(), 1u);
  EXPECT_TRUE(em2.HasComponent<Transform>(e1));
  EXPECT_TRUE(em2.HasComponent<DebugName>(e1));
  const DebugName* name = em2.TryGetComponent<DebugName>(e1);
  EXPECT_STREQ(name->name.c_str(), "EntityOne");
}

TEST_F(FileSerializationTest, ECS_VerifyComponentData) {
  EntityManager em;
  Entity e1 = em.Create();
  Entity e2 = em.Create();
  em.AddComponent<Transform>(e1, VLA::Matrix4x4f::Identity * 3.0f);
  em.AddComponent<Transform>(e2, VLA::Matrix4x4f::Identity * 5.0f);
  em.AddComponent<DebugName>(e1, "First");
  em.AddComponent<DebugName>(e2, "Second");

  std::vector<std::byte> buffer;
  Serializer serializer(buffer);
  em.Serialize(serializer);
  Filesystem::WriteBinary("test_ecs.bin", buffer);

  std::vector<std::byte> readBuffer;
  Filesystem::ReadBinary("test_ecs.bin", readBuffer);

  EntityManager em2;
  Serializer deserializer(readBuffer);
  deserializer.ResetOffset();
  em2.Deserialize(deserializer);

  const Transform* t1 = em2.TryGetComponent<Transform>(e1);
  const Transform* t2 = em2.TryGetComponent<Transform>(e2);
  const DebugName* n1 = em2.TryGetComponent<DebugName>(e1);
  const DebugName* n2 = em2.TryGetComponent<DebugName>(e2);

  ASSERT_NE(t1, nullptr);
  ASSERT_NE(t2, nullptr);
  ASSERT_NE(n1, nullptr);
  ASSERT_NE(n2, nullptr);

  EXPECT_FLOAT_EQ(t1->worldMatrix.A[0], 3.0f);
  EXPECT_FLOAT_EQ(t2->worldMatrix.A[0], 5.0f);
  EXPECT_STREQ(n1->name.c_str(), "First");
  EXPECT_STREQ(n2->name.c_str(), "Second");
}

TEST_F(FileSerializationTest, ECS_DestroyedEntityNotSerialized) {
  EntityManager em;
  Entity e1 = em.Create();
  Entity e2 = em.Create();
  em.AddComponent<Transform>(e1, VLA::Matrix4x4f::Identity);
  em.AddComponent<Transform>(e2, VLA::Matrix4x4f::Identity);
  em.Destroy(e1);

  std::vector<std::byte> buffer;
  Serializer serializer(buffer);
  em.Serialize(serializer);
  Filesystem::WriteBinary("test_ecs.bin", buffer);

  std::vector<std::byte> readBuffer;
  Filesystem::ReadBinary("test_ecs.bin", readBuffer);

  EntityManager em2;
  Serializer deserializer(readBuffer);
  deserializer.ResetOffset();
  em2.Deserialize(deserializer);

  (void)e2;

  EXPECT_EQ(em2.GetEntityCount(), 1u);
  EXPECT_FALSE(em2.IsAlive(e1));
  EXPECT_TRUE(em2.IsAlive(e2));
  EXPECT_NE(nullptr, em2.TryGetComponent<Transform>(e2));
}

TEST_F(FileSerializationTest, ECS_RemoveComponentSerialized) {
  EntityManager em;
  Entity e1 = em.Create();
  em.AddComponent<Transform>(e1, VLA::Matrix4x4f::Identity);
  em.AddComponent<DebugName>(e1, "Test");
  em.TryRemoveComponent<DebugName>(e1);

  std::vector<std::byte> buffer;
  Serializer serializer(buffer);
  em.Serialize(serializer);
  Filesystem::WriteBinary("test_ecs.bin", buffer);

  std::vector<std::byte> readBuffer;
  Filesystem::ReadBinary("test_ecs.bin", readBuffer);

  EntityManager em2;
  Serializer deserializer(readBuffer);
  deserializer.ResetOffset();
  em2.Deserialize(deserializer);

  EXPECT_TRUE(em2.HasComponent<Transform>(e1));
  EXPECT_FALSE(em2.HasComponent<DebugName>(e1));
}

TEST_F(FileSerializationTest, ECS_ManyEntities) {
  constexpr int ENTITY_COUNT = 100;
  EntityManager em;
  std::vector<Entity> entities;
  entities.reserve(ENTITY_COUNT);

  for (int i = 0; i < ENTITY_COUNT; ++i) {
    Entity e = em.Create();
    entities.push_back(e);
    em.AddComponent<Transform>(e, VLA::Matrix4x4f::Identity * static_cast<float>(i + 1));
    em.AddComponent<DebugName>(e, "Entity" + std::to_string(i));
  }

  std::vector<std::byte> buffer;
  Serializer serializer(buffer);
  em.Serialize(serializer);
  Filesystem::WriteBinary("test_ecs.bin", buffer);

  std::vector<std::byte> readBuffer;
  Filesystem::ReadBinary("test_ecs.bin", readBuffer);

  EntityManager em2;
  Serializer deserializer(readBuffer);
  deserializer.ResetOffset();
  em2.Deserialize(deserializer);

  EXPECT_EQ(em2.GetEntityCount(), ENTITY_COUNT);

  for (int i = 0; i < ENTITY_COUNT; ++i) {
    Entity e = entities[i];
    EXPECT_TRUE(em2.IsAlive(e));
    EXPECT_TRUE(em2.HasComponent<Transform>(e));
    EXPECT_TRUE(em2.HasComponent<DebugName>(e));

    const Transform* t = em2.TryGetComponent<Transform>(e);
    EXPECT_FLOAT_EQ(t->worldMatrix.A[0], static_cast<float>(i + 1));

    const DebugName* n = em2.TryGetComponent<DebugName>(e);
    EXPECT_STREQ(n->name.c_str(), ("Entity" + std::to_string(i)).c_str());
  }
}

TEST_F(FileSerializationTest, ECS_DoubleSerializationConsistent) {
  EntityManager em;
  Entity e1 = em.Create();
  em.AddComponent<Transform>(e1, VLA::Matrix4x4f::Identity);
  em.AddComponent<DebugName>(e1, "Test");

  std::vector<std::byte> buffer1;
  Serializer ser1(buffer1);
  em.Serialize(ser1);

  std::vector<std::byte> buffer2;
  Serializer ser2(buffer2);
  em.Serialize(ser2);

  EXPECT_EQ(buffer1.size(), buffer2.size());
  EXPECT_EQ(buffer1, buffer2);
}

TEST_F(FileSerializationTest, ECS_DeserializeIntoNonEmpty) {
  EntityManager em;
  Entity existing = em.Create();
  em.AddComponent<Transform>(existing, VLA::Matrix4x4f::Identity * 10.0f);

  std::vector<std::byte> buffer;
  Serializer serializer(buffer);
  em.Serialize(serializer);

  EntityManager em2;
  Entity e3 = em2.Create();
  em2.AddComponent<Transform>(e3, VLA::Matrix4x4f::Identity * 99.0f);

  Serializer deserializer(buffer);
  deserializer.ResetOffset();
  em2.Deserialize(deserializer);

  EXPECT_EQ(em2.GetEntityCount(), 1u);
  EXPECT_TRUE(em2.IsAlive(existing));

  const Transform* tExisting = em2.TryGetComponent<Transform>(existing);
  ASSERT_NE(tExisting, nullptr);
  EXPECT_FLOAT_EQ(tExisting->worldMatrix.A[0], 10.0f);
}

TEST_F(FileSerializationTest, ECS_GenerationAfterDeserialize) {
  EntityManager em;
  Entity e1 = em.Create();
  em.AddComponent<Transform>(e1, VLA::Matrix4x4f::Identity);
  em.Destroy(e1);
  Entity e2 = em.Create();
  em.AddComponent<Transform>(e2, VLA::Matrix4x4f::Identity * 2.0f);

  std::vector<std::byte> buffer;
  Serializer serializer(buffer);
  em.Serialize(serializer);

  EntityManager em2;
  Serializer deserializer(buffer);
  deserializer.ResetOffset();
  em2.Deserialize(deserializer);

  EXPECT_EQ(em2.GetEntityCount(), 1u);
  EXPECT_TRUE(em2.IsAlive(e2));

  Entity e3 = em2.Create();
  EXPECT_NE(e3.index, e2.index);

  em2.AddComponent<DebugName>(e3, "newlyCreated");
  EXPECT_TRUE(em2.IsAlive(e3));
  EXPECT_NE(nullptr, em2.TryGetComponent<DebugName>(e3));
}

TEST_F(FileSerializationTest, ECS_FreeListPreserved) {
  EntityManager em;
  std::vector<Entity> created;
  for (int i = 0; i < 10; ++i) {
    created.push_back(em.Create());
  }
  for (int i = 0; i < 5; ++i) {
    em.Destroy(created[i]);
  }

  std::vector<std::byte> buffer;
  Serializer serializer(buffer);
  em.Serialize(serializer);

  EntityManager em2;
  Serializer deserializer(buffer);
  deserializer.ResetOffset();
  em2.Deserialize(deserializer);

  EXPECT_EQ(em2.GetEntityCount(), 5u);

  Entity e = em2.Create();
  EXPECT_TRUE(em2.IsAlive(e));
  em2.AddComponent<Transform>(e, VLA::Matrix4x4f::Identity);
  EXPECT_NE(nullptr, em2.TryGetComponent<Transform>(e));
}

TEST_F(FileSerializationTest, ECS_EmptyString) {
  EntityManager em;
  Entity e1 = em.Create();
  em.AddComponent<DebugName>(e1, "");
  em.AddComponent<Transform>(e1, VLA::Matrix4x4f::Identity);

  std::vector<std::byte> buffer;
  Serializer serializer(buffer);
  em.Serialize(serializer);
  Filesystem::WriteBinary("test_ecs.bin", buffer);

  std::vector<std::byte> readBuffer;
  Filesystem::ReadBinary("test_ecs.bin", readBuffer);

  EntityManager em2;
  Serializer deserializer(readBuffer);
  deserializer.ResetOffset();
  em2.Deserialize(deserializer);

  const DebugName* n = em2.TryGetComponent<DebugName>(e1);
  ASSERT_NE(n, nullptr);
  EXPECT_EQ(n->name, "");
}

TEST_F(FileSerializationTest, ECS_LongString) {
  std::string longStr(1000, 'X');
  longStr += "END";

  EntityManager em;
  Entity e1 = em.Create();
  em.AddComponent<DebugName>(e1, longStr);

  std::vector<std::byte> buffer;
  Serializer serializer(buffer);
  em.Serialize(serializer);

  EntityManager em2;
  Serializer deserializer(buffer);
  deserializer.ResetOffset();
  em2.Deserialize(deserializer);

  const DebugName* n = em2.TryGetComponent<DebugName>(e1);
  ASSERT_NE(n, nullptr);
  EXPECT_EQ(n->name, longStr);
}

TEST_F(FileSerializationTest, ECS_DoubleRoundTrip) {
  EntityManager em;
  Entity e1 = em.Create();
  em.AddComponent<Transform>(e1, VLA::Matrix4x4f::Identity);
  em.AddComponent<DebugName>(e1, "Test");

  std::vector<std::byte> buffer1;
  Serializer ser1(buffer1);
  em.Serialize(ser1);

  EntityManager em2;
  Serializer des1(buffer1);
  des1.ResetOffset();
  em2.Deserialize(des1);

  std::vector<std::byte> buffer2;
  Serializer ser2(buffer2);
  em2.Serialize(ser2);

  EXPECT_EQ(buffer1.size(), buffer2.size());
  EXPECT_EQ(buffer1, buffer2);

  EntityManager em3;
  Serializer des2(buffer2);
  des2.ResetOffset();
  em3.Deserialize(des2);

  EXPECT_EQ(em3.GetEntityCount(), 1u);
  EXPECT_TRUE(em3.IsAlive(e1));
  const Transform* t = em3.TryGetComponent<Transform>(e1);
  const DebugName* n = em3.TryGetComponent<DebugName>(e1);
  ASSERT_NE(t, nullptr);
  ASSERT_NE(n, nullptr);
  EXPECT_FLOAT_EQ(t->worldMatrix.A[0], 1.0f);
  EXPECT_EQ(n->name, "Test");
}

TEST_F(FileSerializationTest, ECS_AllComponentTypes) {
  EntityManager em;
  Entity e1 = em.Create();
  em.AddComponent<Transform>(e1, VLA::Matrix4x4f::Identity);
  em.AddComponent<DebugName>(e1, "TestEntity");

  Entity e2 = em.Create();
  em.AddComponent<Renderable>(e2);
  em.GetComponent<Renderable>(e2).meshId = 42;
  em.GetComponent<Renderable>(e2).materialId = 99;
  em.GetComponent<Renderable>(e2).color[0] = 0.5f;

  std::vector<std::byte> buffer;
  Serializer serializer(buffer);
  em.Serialize(serializer);

  EntityManager em2;
  Serializer deserializer(buffer);
  deserializer.ResetOffset();
  em2.Deserialize(deserializer);

  EXPECT_EQ(em2.GetEntityCount(), 2u);

  const Transform* t = em2.TryGetComponent<Transform>(e1);
  const DebugName* n = em2.TryGetComponent<DebugName>(e1);
  const Renderable* r = em2.TryGetComponent<Renderable>(e2);

  ASSERT_NE(t, nullptr);
  ASSERT_NE(n, nullptr);
  ASSERT_NE(r, nullptr);

  EXPECT_FLOAT_EQ(t->worldMatrix.A[0], 1.0f);
  EXPECT_EQ(n->name, "TestEntity");
  EXPECT_EQ(r->meshId, 42u);
  EXPECT_EQ(r->materialId, 99u);
  EXPECT_FLOAT_EQ(r->color[0], 0.5f);
}

TEST_F(FileSerializationTest, ECS_SerializableComponentsOnly) {
  EntityManager em;
  Entity e1 = em.Create();
  em.AddComponent<Transform>(e1, VLA::Matrix4x4f::Identity);
  em.AddComponent<DebugName>(e1, "Test");

  std::vector<std::byte> buffer;
  Serializer serializer(buffer);
  em.Serialize(serializer);

  EntityManager em2;
  Serializer deserializer(buffer);
  deserializer.ResetOffset();
  em2.Deserialize(deserializer);

  auto* transformPool = em2.GetComponentPool<Transform>();
  auto* debugNamePool = em2.GetComponentPool<DebugName>();
  auto* cameraPool = em2.GetComponentPool<Camera>();

  EXPECT_NE(transformPool, nullptr);
  EXPECT_NE(debugNamePool, nullptr);
  EXPECT_EQ(cameraPool, nullptr);
}

TEST_F(FileSerializationTest, ECS_IndependentInstances) {
  EntityManager em1;
  Entity e1 = em1.Create();
  em1.AddComponent<Transform>(e1, VLA::Matrix4x4f::Identity);
  em1.AddComponent<DebugName>(e1, "FirstECS");

  EntityManager em2;
  Entity e2 = em2.Create();
  em2.AddComponent<Transform>(e2, VLA::Matrix4x4f::Identity * 5.0f);
  em2.AddComponent<DebugName>(e2, "SecondECS");

  std::vector<std::byte> buffer1, buffer2;
  Serializer ser1(buffer1);
  Serializer ser2(buffer2);
  em1.Serialize(ser1);
  em2.Serialize(ser2);

  EntityManager em1Restored;
  EntityManager em2Restored;

  Serializer des1(buffer1);
  des1.ResetOffset();
  em1Restored.Deserialize(des1);

  Serializer des2(buffer2);
  des2.ResetOffset();
  em2Restored.Deserialize(des2);

  EXPECT_EQ(em1Restored.GetEntityCount(), 1u);
  EXPECT_EQ(em2Restored.GetEntityCount(), 1u);

  const Transform* t1 = em1Restored.TryGetComponent<Transform>(e1);
  const Transform* t2 = em2Restored.TryGetComponent<Transform>(e2);
  const DebugName* n1 = em1Restored.TryGetComponent<DebugName>(e1);
  const DebugName* n2 = em2Restored.TryGetComponent<DebugName>(e2);

  ASSERT_NE(t1, nullptr);
  ASSERT_NE(t2, nullptr);
  ASSERT_NE(n1, nullptr);
  ASSERT_NE(n2, nullptr);

  EXPECT_FLOAT_EQ(t1->worldMatrix.A[0], 1.0f);
  EXPECT_FLOAT_EQ(t2->worldMatrix.A[0], 5.0f);
  EXPECT_EQ(n1->name, "FirstECS");
  EXPECT_EQ(n2->name, "SecondECS");

  EXPECT_TRUE(em1Restored.IsAlive(e1));
  EXPECT_TRUE(em2Restored.IsAlive(e2));

  Entity newInEM1 = em1Restored.Create();
  Entity newInEM2 = em2Restored.Create();

  EXPECT_EQ(em1Restored.GetEntityCount(), 2u);
  EXPECT_EQ(em2Restored.GetEntityCount(), 2u);

  em1Restored.AddComponent<Transform>(newInEM1, VLA::Matrix4x4f::Identity * 10.0f);
  em2Restored.AddComponent<Transform>(newInEM2, VLA::Matrix4x4f::Identity * 20.0f);

  const Transform* tNew1 = em1Restored.TryGetComponent<Transform>(newInEM1);
  const Transform* tNew2 = em2Restored.TryGetComponent<Transform>(newInEM2);
  ASSERT_NE(tNew1, nullptr);
  ASSERT_NE(tNew2, nullptr);
  EXPECT_FLOAT_EQ(tNew1->worldMatrix.A[0], 10.0f);
  EXPECT_FLOAT_EQ(tNew2->worldMatrix.A[0], 20.0f);

  em1Restored.Destroy(e1);
  EXPECT_EQ(em1Restored.GetEntityCount(), 2u);
  EXPECT_FALSE(em1Restored.IsAlive(e1));
  EXPECT_EQ(em2Restored.GetEntityCount(), 2u);
  EXPECT_TRUE(em2Restored.IsAlive(e2));
}

TEST_F(FileSerializationTest, ECS_DeserializeFirstThenCreate) {
  std::vector<std::byte> buffer;
  {
    EntityManager em;
    Entity e = em.Create();
    em.AddComponent<Transform>(e, VLA::Matrix4x4f::Identity);

    Serializer ser(buffer);
    em.Serialize(ser);
  }

  EntityManager em;
  Serializer des(buffer);
  des.ResetOffset();
  em.Deserialize(des);

  EXPECT_EQ(em.GetEntityCount(), 1u);

  Entity newEntity = em.Create();
  em.AddComponent<Transform>(newEntity, VLA::Matrix4x4f::Identity * 2.0f);
  em.AddComponent<DebugName>(newEntity, "CreatedAfter");

  EXPECT_EQ(em.GetEntityCount(), 2u);
  EXPECT_TRUE(em.IsAlive(newEntity));

  const Transform* t = em.TryGetComponent<Transform>(newEntity);
  const DebugName* n = em.TryGetComponent<DebugName>(newEntity);
  ASSERT_NE(t, nullptr);
  ASSERT_NE(n, nullptr);
  EXPECT_FLOAT_EQ(t->worldMatrix.A[0], 2.0f);
  EXPECT_EQ(n->name, "CreatedAfter");
}

TEST_F(FileSerializationTest, ECS_IdempotentSerialization) {
  EntityManager em;
  Entity e1 = em.Create();
  em.AddComponent<Transform>(e1, VLA::Matrix4x4f::Identity);
  em.AddComponent<DebugName>(e1, "Test");

  std::vector<std::byte> b1, b2, b3;
  Serializer s1(b1), s2(b2), s3(b3);
  em.Serialize(s1);
  em.Serialize(s2);
  em.Serialize(s3);

  EXPECT_EQ(b1, b2);
  EXPECT_EQ(b2, b3);
}

} // namespace SD

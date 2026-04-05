#include <Core/ECS/Commands.hpp>
#include <Core/ECS/EntityManager.hpp>
#include <Core/ECS/CommandQueue.hpp>
#include <Core/ECS/Components.hpp>
#include <gtest/gtest.h>

namespace SD {

class CommandQueueTest : public ::testing::Test {
protected:
  EntityManager em;
  CommandQueue queue;
};

TEST_F(CommandQueueTest, Add_IncreasesCount) {
  EXPECT_EQ(queue.GetCount(), 0u);
  queue.Add<CreateEntityCmd>(EntityHandle(0));
  EXPECT_EQ(queue.GetCount(), 1u);
}

TEST_F(CommandQueueTest, Apply_ExecutesCommands) {
  EntityHandle h(0);
  queue.Add<CreateEntityCmd>(h);
  queue.Apply(em);

  EXPECT_EQ(queue.GetCount(), 0u);
}

TEST_F(CommandQueueTest, CreateEntityCmd_CreatesEntity) {
  EntityHandle h(0);
  queue.Add<CreateEntityCmd>(h);
  queue.Apply(em);

  Entity e = queue.GetEntity(h);
  EXPECT_TRUE(em.IsAlive(e));
}

TEST_F(CommandQueueTest, AddComponentCmd_AddsComponent) {
  EntityHandle h(0);
  queue.Add<CreateEntityCmd>(h);
  queue.Add<AddComponentCmd<Transform>>(h, Transform{VLA::Matrix4x4f::Identity});
  queue.Apply(em);

  Entity e = queue.GetEntity(h);
  EXPECT_TRUE(em.HasComponent<Transform>(e));
}

TEST_F(CommandQueueTest, RemoveComponentCmd_RemovesComponent) {
  EntityHandle h(0);
  queue.Add<CreateEntityCmd>(h);
  queue.Add<AddComponentCmd<Transform>>(h, Transform{VLA::Matrix4x4f::Identity});
  queue.Apply(em);

  Entity e = queue.GetEntity(h);
  EXPECT_TRUE(em.HasComponent<Transform>(e));

  queue.Add<RemoveComponentCmd<Transform>>(h);
  queue.Apply(em);

  EXPECT_FALSE(em.HasComponent<Transform>(e));
}

TEST_F(CommandQueueTest, Clear_RemovesAllCommands) {
  queue.Add<CreateEntityCmd>(EntityHandle(0));
  queue.Add<CreateEntityCmd>(EntityHandle(1));
  EXPECT_EQ(queue.GetCount(), 2u);

  queue.Clear();
  EXPECT_EQ(queue.GetCount(), 0u);
}

class CommandSerializationTest : public ::testing::Test {
protected:
  EntityManager em;
};

TEST_F(CommandSerializationTest, CreateEntityCmd_SerializeDeserialize) {
  CreateEntityCmd cmd(EntityHandle(42));

  std::vector<std::byte> buffer;
  Serializer serializer(buffer);
  cmd.Serialize(serializer);

  EXPECT_GT(buffer.size(), 0u);

  Serializer deserializer(buffer);
  deserializer.ResetOffset();
  CreateEntityCmd cmd2;
  cmd2.Deserialize(deserializer);

  EXPECT_EQ(cmd2.mHandle.id, 42u);
}

TEST_F(CommandSerializationTest, AddComponentCmd_SerializeDeserialize) {
  AddComponentCmd<Transform> cmd;
  cmd.mHandle.id = 5;
  cmd.mData = Transform{VLA::Matrix4x4f::Identity};

  std::vector<std::byte> buffer;
  Serializer serializer(buffer);
  cmd.Serialize(serializer);

  Serializer deserializer(buffer);
  deserializer.ResetOffset();
  AddComponentCmd<Transform> cmd2;
  cmd2.Deserialize(deserializer);

  EXPECT_EQ(cmd2.mHandle.id, 5u);
  EXPECT_EQ(cmd2.mData.worldMatrix, VLA::Matrix4x4f::Identity);
}

TEST_F(CommandSerializationTest, CommandQueue_SerializeDeserialize) {
  CommandQueue queue;
  queue.Add<CreateEntityCmd>(EntityHandle(0));
  queue.Add<CreateEntityCmd>(EntityHandle(1));

  std::vector<std::byte> buffer;
  Serializer serializer(buffer);
  queue.Serialize(serializer);

  CommandQueue queue2;
  Serializer deserializer(buffer);
  deserializer.ResetOffset();
  queue2.Deserialize(deserializer);

  EXPECT_EQ(queue2.GetCount(), 2u);
}
} // namespace SD
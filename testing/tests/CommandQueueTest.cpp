#include <core/ecs/commands.hpp>
#include <core/ecs/EntityManager.hpp>
#include <core/ecs/CommandQueue.hpp>
#include <core/ecs/components.hpp>
#include <gtest/gtest.h>

namespace sd {

class CommandQueueTest : public ::testing::Test {
protected:
  EntityManager em;
  CommandQueue queue;
};

TEST_F(CommandQueueTest, Add_IncreasesCount) {
  EXPECT_EQ(queue.get_count(), 0u);
  queue.add<CreateEntityCmd>(EntityHandle(0));
  EXPECT_EQ(queue.get_count(), 1u);
}

TEST_F(CommandQueueTest, Apply_ExecutesCommands) {
  EntityHandle h(0);
  queue.add<CreateEntityCmd>(h);
  queue.apply(em);

  EXPECT_EQ(queue.get_count(), 0u);
}

TEST_F(CommandQueueTest, CreateEntityCmd_CreatesEntity) {
  EntityHandle h(0);
  queue.add<CreateEntityCmd>(h);
  queue.apply(em);

  Entity e = queue.get_entity(h);
  EXPECT_TRUE(em.is_alive(e));
}

TEST_F(CommandQueueTest, AddComponentCmd_AddsComponent) {
  EntityHandle h(0);
  queue.add<CreateEntityCmd>(h);
  queue.add<AddComponentCmd<Transform>>(h, Transform{VLA::Matrix4x4f::Identity()});
  queue.apply(em);

  Entity e = queue.get_entity(h);
  EXPECT_TRUE(em.has_component<Transform>(e));
}

TEST_F(CommandQueueTest, RemoveComponentCmd_RemovesComponent) {
  EntityHandle h(0);
  queue.add<CreateEntityCmd>(h);
  queue.add<AddComponentCmd<Transform>>(h, Transform{VLA::Matrix4x4f::Identity()});
  queue.apply(em);

  Entity e = queue.get_entity(h);
  EXPECT_TRUE(em.has_component<Transform>(e));

  queue.add<RemoveComponentCmd<Transform>>(h);
  queue.apply(em);

  EXPECT_FALSE(em.has_component<Transform>(e));
}

TEST_F(CommandQueueTest, Clear_RemovesAllCommands) {
  queue.add<CreateEntityCmd>(EntityHandle(0));
  queue.add<CreateEntityCmd>(EntityHandle(1));
  EXPECT_EQ(queue.get_count(), 2u);

  queue.clear();
  EXPECT_EQ(queue.get_count(), 0u);
}

class CommandSerializationTest : public ::testing::Test {
protected:
  EntityManager em;
};

TEST_F(CommandSerializationTest, CreateEntityCmd_SerializeDeserialize) {
  CreateEntityCmd cmd(EntityHandle(42));

  std::vector<std::byte> buffer;
  Serializer serializer(buffer);
  cmd.serialize(serializer);

  EXPECT_GT(buffer.size(), 0u);

  Serializer deserializer(buffer);
  deserializer.reset_offset();
  CreateEntityCmd cmd2;
  cmd2.deserialize(deserializer);

  EXPECT_EQ(cmd2.m_handle.id, 42u);
}

TEST_F(CommandSerializationTest, AddComponentCmd_SerializeDeserialize) {
  AddComponentCmd<Transform> cmd;
  cmd.m_handle.id = 5;
  cmd.m_data = Transform{VLA::Matrix4x4f::Identity()};

  std::vector<std::byte> buffer;
  Serializer serializer(buffer);
  cmd.serialize(serializer);

  Serializer deserializer(buffer);
  deserializer.reset_offset();
  AddComponentCmd<Transform> cmd2;
  cmd2.deserialize(deserializer);

  EXPECT_EQ(cmd2.m_handle.id, 5u);
  EXPECT_EQ(cmd2.m_data.world_matrix, VLA::Matrix4x4f::Identity());
}

TEST_F(CommandSerializationTest, CommandQueue_SerializeDeserialize) {
  CommandQueue queue;
  queue.add<CreateEntityCmd>(EntityHandle(0));
  queue.add<CreateEntityCmd>(EntityHandle(1));

  std::vector<std::byte> buffer;
  Serializer serializer(buffer);
  queue.serialize(serializer);

  CommandQueue queue2;
  Serializer deserializer(buffer);
  deserializer.reset_offset();
  queue2.deserialize(deserializer);

  EXPECT_EQ(queue2.get_count(), 2u);
}
} // namespace sd
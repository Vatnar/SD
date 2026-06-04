#pragma once
#include "Command.hpp"
#include "CommandQueue.hpp"
#include "Entity.hpp"
#include "EntityManager.hpp"
#include "components.hpp"

namespace sd {

class CreateEntityCmd : public Command {
  COMMAND_ID(CreateEntityCmd)
public:
  CreateEntityCmd() = default;
  explicit CreateEntityCmd(EntityHandle handle) : m_handle(handle) {}

  EntityHandle m_handle;
  Entity       m_created_entity = {};

  void execute(EntityManager& em, CommandQueue& queue) override {
    m_created_entity = em.create();
    queue.set_entity_for_handle(m_handle, m_created_entity);
  }
  void serialize(Serializer& serializer) const override { serializer.write(m_handle.id); }
  void deserialize(Serializer& serializer) override { m_handle.id = serializer.read<u32>(); }
};

class DestroyEntityCmd : public Command {
  COMMAND_ID(DestroyEntityCmd)
public:
  Entity m_entity;
  DestroyEntityCmd() = default;
  explicit DestroyEntityCmd(Entity e) : m_entity(e) {}

  void execute(EntityManager& em, CommandQueue&) override { em.destroy(m_entity); }
  void serialize(Serializer& serializer) const override {
    serializer.write<u32>(m_entity.index);
    serializer.write<u32>(m_entity.generation);
  }
  void deserialize(Serializer& serializer) override {
    m_entity.index      = serializer.read<u32>();
    m_entity.generation = serializer.read<u32>();
  }
};

template<SerializableComponent T>
class AddComponentCmd : public Command {
  COMMAND_ID_T(AddComponentCmd, T)
public:
  EntityHandle m_handle;
  T            m_data;
  AddComponentCmd() = default;
  AddComponentCmd(EntityHandle handle, T data) : m_handle(handle), m_data(data) {}

  void execute(EntityManager& em, CommandQueue& queue) override {
    Entity e = queue.get_entity(m_handle);
    em.add_component<T>(e, m_data);
  }

  void serialize(Serializer& serializer) const override {
    serializer.write(m_handle.id);
    ComponentSerializer<T>::serialize(m_data, serializer);
  }

  void deserialize(Serializer& serializer) override {
    m_handle.id = serializer.read<u32>();
    ComponentSerializer<T>::deserialize(m_data, serializer);
  }
};

template<SerializableComponent T>
class RemoveComponentCmd : public Command {
  COMMAND_ID_T(RemoveComponentCmd, T)
public:
  EntityHandle m_handle;
  RemoveComponentCmd() = default;
  explicit RemoveComponentCmd(EntityHandle handle) : m_handle(handle) {}

  void execute(EntityManager& em, CommandQueue& queue) override {
    Entity e = queue.get_entity(m_handle);
    em.try_remove_component<T>(e);
  }
  void serialize(Serializer& serializer) const override { serializer.write(m_handle.id); }
  void deserialize(Serializer& serializer) override { m_handle.id = serializer.read<u32>(); }
};

REGISTER_COMMAND(CreateEntityCmd);
REGISTER_COMMAND(DestroyEntityCmd);
REGISTER_COMPONENT_COMMANDS(Transform);
REGISTER_COMPONENT_COMMANDS(Camera);
REGISTER_COMPONENT_COMMANDS(Renderable);
REGISTER_COMPONENT_COMMANDS(DebugName);

} // namespace sd

#pragma once
#include "Command.hpp"
#include "CommandQueue.hpp"
#include "Entity.hpp"
#include "EntityManager.hpp"
#include "components.hpp"

namespace sd {

struct CreateEntityCmd {
  EntityHandle m_handle;
  Entity       m_created_entity = {};

  void execute(EntityManager<ComponentGroup<>>& em, CommandQueue& queue) {
    m_created_entity = em.create();
    queue.set_entity_for_handle(m_handle, m_created_entity);
  }
  void serialize(Serializer& serializer) const { serializer.write(m_handle.id); }
  void deserialize(Serializer& serializer) { m_handle.id = serializer.read<U32>(); }
};

struct DestroyEntityCmd {
  Entity m_entity;

  void execute(EntityManager<ComponentGroup<>>& em, CommandQueue&) { em.destroy(m_entity); }
  void serialize(Serializer& serializer) const {
    serializer.write<U32>(m_entity.index);
    serializer.write<U32>(m_entity.generation);
  }
  void deserialize(Serializer& serializer) {
    m_entity.index      = serializer.read<U32>();
    m_entity.generation = serializer.read<U32>();
  }
};

template<SerializableComponent T>
struct AddComponentCmd {
  EntityHandle m_handle;
  T            m_data;

  AddComponentCmd() = default;
  AddComponentCmd(EntityHandle handle, T data) : m_handle(handle), m_data(data) {}

  void execute(EntityManager<ComponentGroup<>>& em, CommandQueue& queue) {
    Entity e = queue.get_entity(m_handle);
    em.add_component<T>(e, m_data);
  }

  void serialize(Serializer& serializer) const {
    serializer.write(m_handle.id);
    ComponentSerializer<T>::serialize(m_data, serializer);
  }

  void deserialize(Serializer& serializer) {
    m_handle.id = serializer.read<U32>();
    ComponentSerializer<T>::deserialize(m_data, serializer);
  }
};

template<SerializableComponent T>
struct RemoveComponentCmd {
  EntityHandle m_handle;

  explicit RemoveComponentCmd(EntityHandle handle) : m_handle(handle) {}

  void execute(EntityManager<ComponentGroup<>>& em, CommandQueue& queue) {
    Entity e = queue.get_entity(m_handle);
    em.try_remove_component<T>(e);
  }
  void serialize(Serializer& serializer) const { serializer.write(m_handle.id); }
  void deserialize(Serializer& serializer) { m_handle.id = serializer.read<U32>(); }
};

} // namespace sd

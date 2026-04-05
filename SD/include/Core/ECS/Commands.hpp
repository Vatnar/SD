#pragma once
#include "Command.hpp"
#include "CommandQueue.hpp"
#include "Components.hpp"
#include "Entity.hpp"
#include "EntityManager.hpp"

namespace SD {

class CreateEntityCmd : public Command {
  COMMAND_ID(CreateEntityCmd)
public:
  CreateEntityCmd() = default;
  explicit CreateEntityCmd(EntityHandle handle) : mHandle(handle) {}

  EntityHandle mHandle;
  Entity mCreatedEntity = {};

  void Execute(EntityManager& em, CommandQueue& queue) override {
    mCreatedEntity = em.Create();
    queue.SetEntityForHandle(mHandle, mCreatedEntity);
  }
  void Serialize(Serializer& serializer) const override { serializer.Write(mHandle.id); }
  void Deserialize(Serializer& serializer) override { mHandle.id = serializer.Read<u32>(); }
};
REGISTER_COMMAND_FACTORY(CreateEntityCmd);

class DestroyEntityCmd : public Command {
  COMMAND_ID(DestroyEntityCmd)
public:
  Entity mEntity;
  DestroyEntityCmd() = default;
  explicit DestroyEntityCmd(Entity e) : mEntity(e) {}

  void Execute(EntityManager& em, CommandQueue&) override { em.Destroy(mEntity); }
  void Serialize(Serializer& serializer) const override {
    serializer.Write<u32>(mEntity.index);
    serializer.Write<u32>(mEntity.generation);
  }
  void Deserialize(Serializer& serializer) override {
    mEntity.index = serializer.Read<u32>();
    mEntity.generation = serializer.Read<u32>();
  }
};
REGISTER_COMMAND_FACTORY(DestroyEntityCmd);

template<SerializableComponent T>
class AddComponentCmd : public Command {
  COMMAND_ID_T(AddComponentCmd, T)
public:
  EntityHandle mHandle;
  T mData;
  AddComponentCmd() = default;
  AddComponentCmd(EntityHandle handle, T data) : mHandle(handle), mData(data) {}

  void Execute(EntityManager& em, CommandQueue& queue) override {
    Entity e = queue.GetEntity(mHandle);
    em.AddComponent<T>(e, mData);
  }

  void Serialize(Serializer& serializer) const override {
    serializer.Write(mHandle.id);
    ComponentSerializer<T>::Serialize(mData, serializer);
  }

  void Deserialize(Serializer& serializer) override {
    mHandle.id = serializer.Read<u32>();
    ComponentSerializer<T>::Deserialize(mData, serializer);
  }
};

template<SerializableComponent T>
class RemoveComponentCmd : public Command {
  COMMAND_ID_T(RemoveComponentCmd, T)
public:
  EntityHandle mHandle;
  RemoveComponentCmd() = default;
  explicit RemoveComponentCmd(EntityHandle handle) : mHandle(handle) {}

  void Execute(EntityManager& em, CommandQueue& queue) override {
    Entity e = queue.GetEntity(mHandle);
    em.TryRemoveComponent<T>(e);
  }
  void Serialize(Serializer& serializer) const override { serializer.Write(mHandle.id); }
  void Deserialize(Serializer& serializer) override { mHandle.id = serializer.Read<u32>(); }
};

} // namespace SD

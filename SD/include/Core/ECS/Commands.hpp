#pragma once
#include "Command.hpp"
#include "CommandQueue.hpp"
#include "Entity.hpp"
#include "EntityManager.hpp"

namespace SD {

class CreateEntityCmd : public Command {
  EntityHandle mHandle;
  Entity mCreatedEntity = {};

public:
  void Execute(EntityManager& em, CommandQueue& queue) override {
    mCreatedEntity = em.Create();
    queue.SetEntityForHandle(mHandle, mCreatedEntity);
  }
  void Serialize(Serializer& serializer) const override { serializer.Write(mHandle.id); }
  void Deserialize(Serializer& serializer) override { mHandle.id = serializer.Read<u32>(); }
};

class DestroyEntityCmd : public Command {
public:
  Entity mEntity;
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
template<SerializableComponent T>
class AddComponentCmd : public Command {
public:
  EntityHandle mHandle;
  T mData; // maybe force it to be registered somehow
  void Execute(EntityManager& em, CommandQueue& queue) override {
    Entity e = queue.GetEntity(mHandle);
    em.AddComponent(e, mData);
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

/**
 * Silently fails if entity doesn't have specified component
 */
template<SerializableComponent T>
class RemoveComponentCmd : public Command {
public:
  EntityHandle mHandle;
  void Execute(EntityManager& em, CommandQueue& queue) override {
    Entity e = queue.GetEntity(mHandle);
    em.TryRemoveComponent<T>(e);
  }
  void Serialize(Serializer& serializer) const override { serializer.Write(mHandle.id); }
  void Deserialize(Serializer& serializer) override { mHandle.id = serializer.Read<u32>(); }
};


} // namespace SD

#pragma once
#include "Command.hpp"
#include "Entity.hpp"

namespace SD {

class CommandQueue {
public:
  template<typename T, typename... Args>
  void Add(Args&&... args) {
    mCommands.push_back(std::make_unique<T>(std::forward<Args>(args)...));
  }
  void Apply(EntityManager& em);

  [[nodiscard]] Entity GetEntity(EntityHandle handle) const;

  void SetEntityForHandle(EntityHandle entityHandle, Entity entity);
  void Clear();
  [[nodiscard]] usize GetCount() const;

  void Serialize(Serializer& serializer) const;
  void Deserialize(Serializer& serializer);

private:
  std::vector<std::unique_ptr<Command>> mCommands;
  std::vector<Entity> mHandleToEntity;
  std::mutex mMutex;
};

} // namespace SD

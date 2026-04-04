#include "Core/ECS/CommandQueue.hpp"

namespace SD {


void CommandQueue::Apply(EntityManager& em) {
  // Shouldn't really be full, but to be safe against invariants
  mHandleToEntity.clear();
  for (const auto& cmd : mCommands) {
    cmd->Execute(em, *this);
  }
  mHandleToEntity.clear();
}
void CommandQueue::SetEntityForHandle(EntityHandle entityHandle, Entity entity) {
  mHandleToEntity[entityHandle.id] = entity;
}
void CommandQueue::Clear() {
  mCommands.clear();
}
usize CommandQueue::GetCount() const {
  return mCommands.size();
}
Entity CommandQueue::GetEntity(EntityHandle handle) const {
  assert(handle.id < mHandleToEntity.size() && "Entity has not been resolved");
  return mHandleToEntity[handle.id];
}
} // namespace SD

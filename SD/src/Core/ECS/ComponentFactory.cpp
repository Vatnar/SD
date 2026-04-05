#include "Core/ECS/ComponentFactory.hpp"

namespace SD {

void ComponentFactory::Register(u32 componentId, PoolCreatorFunc creator) {
  if (componentId >= creators.size()) {
    creators.resize(componentId + 1);
  }
  creators[componentId] = std::move(creator);
}

std::unique_ptr<SparseEntitySetBase> ComponentFactory::Create(u32 componentId) {
  if (componentId < creators.size() && creators[componentId]) {
    return creators[componentId]();
  }
  return nullptr;
}

bool ComponentFactory::IsRegistered(u32 componentId) {
  return componentId < creators.size() && creators[componentId] != nullptr;
}

} // namespace SD
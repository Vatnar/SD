#include "SD/core/ecs/ComponentFactory.hpp"

#include "SD/core/ecs/component_registration.hpp"
#include "SD/core/ecs/components.hpp"

namespace sd {

void ComponentFactory::register_component(U32 component_id, PoolCreatorFn creator) {
  if (component_id >= m_creators.size()) {
    m_creators.resize(component_id + 1);
  }
  m_creators[component_id] = std::move(creator);
}

std::unique_ptr<SparseEntitySetBase> ComponentFactory::create(U32 component_id) {
  if (component_id < m_creators.size() && m_creators[component_id]) {
    return m_creators[component_id]();
  }
  return nullptr;
}

bool ComponentFactory::is_registered(U32 component_id) {
  return component_id < m_creators.size() && m_creators[component_id] != nullptr;
}

void ComponentFactory::clear() {
  m_creators.clear();
  // TODO: reset ComponentIdGenerator when serialization system is rebuilt
}

void ComponentFactory::register_default_pools() {
  // TODO: re-implement when serialization system is rebuilt
  // register_component(ComponentTraits<Transform>::id, ...)
}

} // namespace sd

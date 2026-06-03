#include "SD/core/ecs/ComponentFactory.hpp"

namespace sd {

void ComponentFactory::register_component(u32 component_id, PoolCreatorFn creator) {
  if (component_id >= m_creators.size()) {
    m_creators.resize(component_id + 1);
  }
  m_creators[component_id] = std::move(creator);
}

std::unique_ptr<SparseEntitySetBase> ComponentFactory::create(u32 component_id) {
  if (component_id < m_creators.size() && m_creators[component_id]) {
    return m_creators[component_id]();
  }
  return nullptr;
}

bool ComponentFactory::is_registered(u32 component_id) {
  return component_id < m_creators.size() && m_creators[component_id] != nullptr;
}

} // namespace sd
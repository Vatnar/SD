#include "SD/core/ecs/ComponentFactory.hpp"

#include "SD/core/ecs/component_registration.hpp"
#include "SD/core/ecs/components.hpp"

namespace sd {

void ComponentFactory::register_component(U32 component_id, PoolNodeCreatorFn creator) {
  if (component_id >= m_creators.size())
    m_creators.resize(component_id + 1);
  m_creators[component_id] = creator;
}

ComponentPoolNode ComponentFactory::create(U32 component_id, Arena* arena) {
  if (component_id < m_creators.size() && m_creators[component_id])
    return m_creators[component_id](arena);
  return {};
}

bool ComponentFactory::is_registered(U32 component_id) {
  return component_id < m_creators.size() && m_creators[component_id] != nullptr;
}

void ComponentFactory::clear() {
  m_creators.clear();
}

void ComponentFactory::register_default_pools() {
}

} // namespace sd

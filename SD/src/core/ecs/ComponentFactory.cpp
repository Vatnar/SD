#include "SD/core/ecs/ComponentFactory.hpp"

#include "SD/core/ecs/component_registration.hpp"
#include "SD/core/ecs/components.hpp"

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

void ComponentFactory::clear() {
  m_creators.clear();
  detail::ComponentIdGenerator::reset();
}

void ComponentFactory::register_default_pools() {
  register_component(ComponentTraits<Transform>::id,
                     [] { return std::make_unique<SparseEntitySet<Transform>>(); });
  register_component(ComponentTraits<Camera>::id,
                     [] { return std::make_unique<SparseEntitySet<Camera>>(); });
  register_component(ComponentTraits<Renderable>::id,
                     [] { return std::make_unique<SparseEntitySet<Renderable>>(); });
  register_component(ComponentTraits<DebugName>::id,
                     [] { return std::make_unique<SparseEntitySet<DebugName>>(); });
}

} // namespace sd

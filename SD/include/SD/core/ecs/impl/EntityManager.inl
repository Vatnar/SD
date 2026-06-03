#pragma once
template<typename... Components>
ViewImpl<Components...>::ViewImpl(EntityManager& manager) : m_manager(manager) {
  usize min_size = std::numeric_limits<usize>::max();

  (check_size<Components>(min_size), ...);
}
template<typename... Components>
ViewImpl<Components...>::Iterator::Iterator(EntityManager&             em,
                                            const std::vector<Entity>* dense_entities, usize idx) :
  manager(em), entities(dense_entities), index(idx) {
  if (entities && index < entities->size() && !is_valid()) {
    next();
  }
}
template<typename... Components>
ViewImpl<Components...>::Iterator& ViewImpl<Components...>::Iterator::operator++() {
  next();
  return *this;
}
template<typename... Components>
std::tuple<Entity, Components&...> ViewImpl<Components...>::Iterator::operator*() const {
  Entity current_entity = (*entities)[index];
  return std::tuple_cat(std::make_tuple(current_entity),
                        manager.get_component_group<Components...>(current_entity));
}
template<typename... Components>
void ViewImpl<Components...>::Iterator::next() {
  if (!entities)
    return;
  do {
    index++;
  } while (index < entities->size() && !is_valid());
}
template<typename... Components>
bool ViewImpl<Components...>::Iterator::is_valid() const {
  if (!entities)
    return false;
  Entity current_entity = (*entities)[index];
  return (manager.has_component<Components>(current_entity) && ...);
}
template<typename... Components>
ViewImpl<Components...>::Iterator ViewImpl<Components...>::begin() {
  if (!m_smallest_pool) {
    log::engine::warn(
        "View has no valid component pools - scene may be empty or missing components");
    return end();
  }
  return Iterator(m_manager, m_smallest_pool, 0);
}
template<typename... Components>
ViewImpl<Components...>::Iterator ViewImpl<Components...>::end() {
  if (!m_smallest_pool) {
    return Iterator(m_manager, nullptr, 0);
  }
  return Iterator(m_manager, m_smallest_pool, m_smallest_pool->size());
}
template<typename... Components>
template<typename Component>
void ViewImpl<Components...>::check_size(usize& minSize) {
  if (!m_manager.has_component_pool<Component>()) {
    minSize         = 0;
    m_smallest_pool = nullptr;
    return;
  }

  auto* pool = m_manager.get_component_pool<Component>();
  if (pool->size() < minSize) {
    minSize         = pool->size();
    m_smallest_pool = &pool->get_dense_entities();
  }
}


template<typename T, typename... Args>
T* EntityManager::add_component(Entity e, Args&&... args) {
  static_assert(ComponentTraits<T>::s_is_registered,
                "Error: Component type is not registered, register it");
  const usize type_id = ComponentTraits<T>::id;


  if (type_id >= m_component_pools.size())
    m_component_pools.resize(type_id + 1);
  if (!m_component_pools[type_id])
    m_component_pools[type_id] = std::make_unique<SparseEntitySet<T>>(); // TODO: arena here

  // add data
  if (m_entity_masks[e]->test(type_id)) {
    log::engine::warn("Overwriting already existing component: {}, id: {} ",
                      ComponentTraits<T>::name, type_id);
  }
  auto* pool = static_cast<SparseEntitySet<T>*>(m_component_pools[type_id].get());
  m_entity_masks[e]->set(type_id);
  pool->add(e, std::forward<Args>(args)...);

  return pool->get(e);
}
template<typename T>
T* EntityManager::try_get_component(Entity e) {
  static_assert(ComponentTraits<T>::s_is_registered,
                "Error: Component type is not registered, register it");

  usize type_id = ComponentTraits<T>::id;
  if (type_id >= m_component_pools.size() || !m_component_pools[type_id] ||
      !m_entity_masks[e]->test(type_id))
    return nullptr;

  auto* pool = static_cast<SparseEntitySet<T>*>(m_component_pools[type_id].get());
  return pool->get(e);
}
template<typename T>
T& EntityManager::get_component(Entity e) {
  usize typeId = ComponentTraits<T>::id;
  assert(has_component<T>(e) && "Entity doesnt have component");

  auto* pool = static_cast<SparseEntitySet<T>*>(m_component_pools[typeId].get());
  return *pool->get(e);
}
template<typename T>
const T& EntityManager::get_component(Entity e) const {
  usize type_id = ComponentTraits<T>::id;
  assert(has_component<T>(e) && "Entity doesnt have component");

  auto* pool = static_cast<const SparseEntitySet<T>*>(m_component_pools[type_id].get());
  return *pool->get(e);
}

template<typename T>
bool EntityManager::try_remove_component(Entity e) {
  static_assert(ComponentTraits<T>::s_is_registered,
                "Error: Can't remove component type that isn't registered");

  usize type_id = ComponentTraits<T>::id;
  if (type_id >= m_component_pools.size() || !m_component_pools[type_id] ||
      !m_entity_masks[e]->test(type_id))
    return false;

  auto* pool = static_cast<SparseEntitySet<T>*>(m_component_pools[type_id].get());
  pool->remove(e);
  m_entity_masks[e]->reset(type_id);
  return true;
}


template<typename... Args>
auto EntityManager::view() {
  if constexpr (sizeof...(Args) == 1) {
    using T = std::tuple_element_t<0, std::tuple<Args...>>;
    if constexpr (UnpackGroup<T>::is_group) {
      return typename UnpackGroup<T>::type(*this);
    } else {
      return ViewImpl<T>(*this);
    }
  } else {
    return ViewImpl<Args...>(*this);
  }
}
template<typename T>
bool EntityManager::has_component(Entity e) const {
  if (e.index >= m_entity_masks.get_dense_entities().size())
    return false;

  if (!is_alive(e))
    return false;

  auto mask = m_entity_masks.get(e);
  return mask && mask->test(ComponentTraits<T>::id);
}
template<typename... Components>
std::tuple<Components&...> EntityManager::get_component_group(Entity e) {
  return std::forward_as_tuple(get_component<Components>(e)...);
}
template<typename... Components>
std::tuple<const Components&...> EntityManager::get_component_group(Entity e) const {
  return std::forward_as_tuple(*get_component<Components>(e)...);
}
template<typename T>
SparseEntitySet<T>* EntityManager::get_component_pool() {
  static_assert(ComponentTraits<T>::s_is_registered != false, "Unregistered Component");
  const usize type_id = ComponentTraits<T>::id;
  if (type_id >= m_component_pools.size() || !m_component_pools[type_id])
    return nullptr;

  return static_cast<SparseEntitySet<T>*>(m_component_pools[type_id].get());
}
template<typename T>
bool EntityManager::has_component_pool() {
  static_assert(ComponentTraits<T>::s_is_registered != false, "Unregistered Component");
  const usize type_id = ComponentTraits<T>::id;
  return type_id < m_component_pools.size() && m_component_pools[type_id];
}
inline Entity EntityManager::create() {
  const uint32_t idx = m_free_list.empty() ? m_generations.size() : pop_free_list();

  if (idx >= m_generations.size()) {
    m_generations.resize(idx + 1, 0);
  }
  Entity e = {idx, m_generations[idx]};

  m_entity_masks.add(e, ComponentMask{});
#ifndef NDEBUG
  ValidateInvariants();
#endif
  return e;
}
inline void EntityManager::destroy(const Entity e) {
  if (!is_alive(e))
    return;


  // TODO: Ranges?
  ComponentMask* mask = m_entity_masks[e];
  assert(mask);

  for (usize i = 0; i < mask->size(); ++i) {
    if (mask->test(i)) {
      if (m_component_pools[i])
        m_component_pools[i]->remove(e);
    }
  }
  mask->reset();
  m_generations[e.index]++;
  m_free_list.push_back(e.index);
#ifndef NDEBUG
  ValidateInvariants();
#endif
}
inline std::vector<ComponentDebugInfo> EntityManager::get_all_component_info(Entity e) const {
  std::vector<ComponentDebugInfo> components;
  if (!is_alive(e))
    return {};
  for (auto& pool : m_component_pools) {
    if (!pool)
      continue;

    if (auto info = pool->get_debug_info(e)) {
      components.push_back(info.value()); // arena
    }
  }
  return components;
}
inline bool EntityManager::is_alive(const Entity e) const {
  return e.index < m_generations.size() && m_generations[e.index] == e.generation;
}
inline uint32_t EntityManager::pop_free_list() {
  const auto idx = m_free_list.back();
  m_free_list.pop_back();
  return idx;
}

inline void EntityManager::serialize(Serializer& s) const {
  s.write(m_generations);
  s.write(m_free_list);

  // Serialize entity masks - only serialize alive entities
  const auto& mask_entities = m_entity_masks.get_dense_entities();
  u32         aliveCount    = 0;
  for (Entity e : mask_entities) {
    if (is_alive(e))
      aliveCount++;
  }
  s.write(aliveCount);

  for (Entity e : mask_entities) {
    if (!is_alive(e))
      continue;

    s.write(e.index);
    s.write(e.generation);
    const ComponentMask* mask = m_entity_masks.get(e);
    // Pack 256-bit mask into 4 x u64 values (instead of 256 x u64)
    if (mask) {
      for (usize word = 0; word < 4; ++word) {
        u64 packed = 0;
        for (usize bit = 0; bit < 64; ++bit) {
          if (mask->test(word * 64 + bit)) {
            packed |= (u64(1) << bit);
          }
        }
        s.write(packed);
      }
    } else {
      for (usize i = 0; i < 4; ++i) {
        s.write(static_cast<u64>(0));
      }
    }
  }

  // Serialize component pools - only registered serializable ones
  u32 serializable_count = 0;
  for (u32 i = 0; i < m_component_pools.size(); ++i) {
    if (m_component_pools[i] && sd::ComponentFactory::is_registered(i)) {
      serializable_count++;
    }
  }
  s.write(serializable_count);

  for (u32 i = 0; i < m_component_pools.size(); ++i) {
    if (m_component_pools[i] && sd::ComponentFactory::is_registered(i)) {
      s.write(i); // component type ID
      m_component_pools[i]->serialize(s);
    }
  }
}

inline void EntityManager::deserialize(Serializer& s) {
  s.read(m_generations);
  s.read(m_free_list);

  // Deserialize alive entity masks
  u32 maskCount = s.read<u32>();
  for (u32 i = 0; i < maskCount; ++i) {
    u32    index      = s.read<u32>();
    u32    generation = s.read<u32>();
    Entity e{index, generation};
    m_entity_masks.add(e, ComponentMask{});
    ComponentMask* mask = m_entity_masks.get(e);
    // Read 4 x u64 values and unpack to 256-bit mask
    if (mask) {
      for (usize word = 0; word < 4; ++word) {
        u64 packed = s.read<u64>();
        for (usize bit = 0; bit < 64; ++bit) {
          if (packed & (static_cast<u64>(1) << bit)) {
            mask->set(word * 64 + bit);
          }
        }
      }
    } else {
      for (usize j = 0; j < 4; ++j) {
        s.read<u64>();
      }
    }
  }

  // Reconstruct mask entries for destroyed entities (needed for invariants)
  for (u32 idx : m_free_list) {
    assert(m_generations[idx] > 0 && "Freelist index has generation 0");
    Entity e{idx, m_generations[idx] - 1};
    m_entity_masks.add(e, ComponentMask{});
  }

  // Deserialize component pools
  u32 serializable_count = s.read<u32>();
  m_component_pools.resize(16); // Basic size, expand as needed

  for (u32 i = 0; i < serializable_count; ++i) {
    u32  componentId = s.read<u32>();
    auto pool        = sd::ComponentFactory::create(componentId);
    if (pool) {
      pool->deserialize(s);
      if (componentId < m_component_pools.size()) {
        m_component_pools[componentId] = std::move(pool);
      }
    }
  }
}

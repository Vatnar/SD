#pragma once

template<typename ExtraComponents, typename... Components>
ViewImpl<ExtraComponents, Components...>::ViewImpl(EntityManager<ExtraComponents>& manager) :
  m_manager(manager) {
  USize min_size = std::numeric_limits<USize>::max();
  (check_size<Components>(min_size), ...);
}

template<typename ExtraComponents, typename... Components>
ViewImpl<ExtraComponents, Components...>::Iterator::Iterator(EntityManager<ExtraComponents>& em,
                                                             const ArenaVec<Entity>* dense_entities,
                                                             USize                   idx) :
  manager(em), entities(dense_entities), index(idx) {
  if (entities && index < entities->count && !is_valid())
    next();
}

template<typename ExtraComponents, typename... Components>
ViewImpl<ExtraComponents, Components...>::Iterator&
ViewImpl<ExtraComponents, Components...>::Iterator::operator++() {
  next();
  return *this;
}

template<typename ExtraComponents, typename... Components>
std::tuple<Entity, Components&...>
ViewImpl<ExtraComponents, Components...>::Iterator::operator*() const {
  Entity current_entity = (*entities)[index];
  return std::tuple_cat(std::make_tuple(current_entity),
                        manager.template get_component_group<Components...>(current_entity));
}

template<typename ExtraComponents, typename... Components>
void ViewImpl<ExtraComponents, Components...>::Iterator::next() {
  if (!entities)
    return;
  do {
    index++;
  } while (index < entities->count && !is_valid());
}

template<typename ExtraComponents, typename... Components>
bool ViewImpl<ExtraComponents, Components...>::Iterator::is_valid() const {
  if (!entities)
    return false;
  Entity current_entity = (*entities)[index];
  return (manager.template has_component<Components>(current_entity) && ...);
}

template<typename ExtraComponents, typename... Components>
typename ViewImpl<ExtraComponents, Components...>::Iterator
ViewImpl<ExtraComponents, Components...>::begin() {
  if (!m_smallest_pool) {
    log::engine::warn(
        "View has no valid component pools - scene may be empty or missing components");
    return end();
  }
  return Iterator(m_manager, m_smallest_pool, 0);
}

template<typename ExtraComponents, typename... Components>
typename ViewImpl<ExtraComponents, Components...>::Iterator
ViewImpl<ExtraComponents, Components...>::end() {
  if (!m_smallest_pool)
    return Iterator(m_manager, nullptr, 0);
  return Iterator(m_manager, m_smallest_pool, m_smallest_pool->count);
}

template<typename ExtraComponents, typename... Components>
template<typename Component>
void ViewImpl<ExtraComponents, Components...>::check_size(USize& minSize) {
  if (!m_manager.template has_component_pool<Component>()) {
    minSize         = 0;
    m_smallest_pool = nullptr;
    return;
  }

  auto* pool = m_manager.template get_component_pool<Component>();
  if (pool->size() < minSize) {
    minSize         = pool->size();
    m_smallest_pool = &pool->get_dense_entities();
  }
}

template<typename ExtraComponents>
template<typename T, typename... Args>
T* EntityManager<ExtraComponents>::add_component(Entity e, Args&&... args) {
  static_assert(component_info<T>::is_registered,
                "Error: Component type is not registered, register it");
  const USize type_id = component_info<T>::id();

  while (m_component_pools.count <= type_id)
    m_component_pools.push(m_pool_arena, ComponentPoolNode{});

  auto& node = m_component_pools[type_id];
  if (!node.pool) {
    auto* pool     = m_pool_arena->push_array<SparseEntitySet<T>>(1);
    pool->arena    = m_pool_arena;
    node.pool      = pool;
    node.remove_fn = [](void* p, Entity e) -> bool {
      return static_cast<SparseEntitySet<T>*>(p)->remove(e);
    };
    node.serialize_fn = [](void* p, Serializer& s) {
      static_cast<SparseEntitySet<T>*>(p)->serialize(s);
    };
    node.deserialize_fn = [](void* p, Serializer& s) {
      static_cast<SparseEntitySet<T>*>(p)->deserialize(s);
    };
  }

  auto* pool = static_cast<SparseEntitySet<T>*>(node.pool);
  if (m_entity_masks.get(e)->test(type_id))
    log::engine::warn("Overwriting already existing component: {}, id: {} ",
                      component_info<T>::name,
                      type_id);

  m_entity_masks.get(e)->set(type_id);
  pool->add(e, std::forward<Args>(args)...);
  return pool->get(e);
}

template<typename ExtraComponents>
template<typename T>
T* EntityManager<ExtraComponents>::try_get_component(Entity e) {
  static_assert(component_info<T>::is_registered,
                "Error: Component type is not registered, register it");

  USize type_id = component_info<T>::id();
  if (type_id >= m_component_pools.count || !m_component_pools[type_id].pool ||
      !m_entity_masks.get(e)->test(type_id))
    return nullptr;

  auto* pool = static_cast<SparseEntitySet<T>*>(m_component_pools[type_id].pool);
  return pool->get(e);
}

template<typename ExtraComponents>
template<typename T>
T& EntityManager<ExtraComponents>::get_component(Entity e) {
  USize typeId = component_info<T>::id();
  assert(has_component<T>(e) && "Entity doesnt have component");

  auto* pool = static_cast<SparseEntitySet<T>*>(m_component_pools[typeId].pool);
  return *pool->get(e);
}

template<typename ExtraComponents>
template<typename T>
const T& EntityManager<ExtraComponents>::get_component(Entity e) const {
  USize type_id = component_info<T>::id();
  assert(has_component<T>(e) && "Entity doesnt have component");

  auto* pool = static_cast<const SparseEntitySet<T>*>(m_component_pools[type_id].pool);
  return *pool->get(e);
}

template<typename ExtraComponents>
template<typename T>
bool EntityManager<ExtraComponents>::try_remove_component(Entity e) {
  static_assert(component_info<T>::is_registered,
                "Error: Can't remove component type that isn't registered");

  USize type_id = component_info<T>::id();
  if (type_id >= m_component_pools.count || !m_component_pools[type_id].pool ||
      !m_entity_masks.get(e)->test(type_id))
    return false;

  auto* pool = static_cast<SparseEntitySet<T>*>(m_component_pools[type_id].pool);
  pool->remove(e);
  m_entity_masks.get(e)->reset(type_id);
  return true;
}

template<typename ExtraComponents>
template<typename... Args>
auto EntityManager<ExtraComponents>::view() {
  if constexpr (sizeof...(Args) == 1) {
    using T = std::tuple_element_t<0, std::tuple<Args...>>;
    if constexpr (UnpackGroup<T>::is_group) {
      return typename UnpackGroup<T>::type(*this);
    } else {
      return ViewImpl<ExtraComponents, T>(*this);
    }
  } else {
    return ViewImpl<ExtraComponents, Args...>(*this);
  }
}

template<typename ExtraComponents>
template<typename T>
bool EntityManager<ExtraComponents>::has_component(Entity e) const {
  if (e.index >= m_entity_masks.get_dense_entities().count)
    return false;
  if (!is_alive(e))
    return false;
  auto mask = m_entity_masks.get(e);
  return mask && mask->test(component_info<T>::id());
}

template<typename ExtraComponents>
template<typename... Components>
std::tuple<Components&...> EntityManager<ExtraComponents>::get_component_group(Entity e) {
  return std::forward_as_tuple(get_component<Components>(e)...);
}

template<typename ExtraComponents>
template<typename... Components>
std::tuple<const Components&...>
EntityManager<ExtraComponents>::get_component_group(Entity e) const {
  return std::forward_as_tuple(get_component<Components>(e)...);
}

template<typename ExtraComponents>
template<typename T>
SparseEntitySet<T>* EntityManager<ExtraComponents>::get_component_pool() {
  static_assert(component_info<T>::is_registered != false, "Unregistered Component");
  const USize type_id = component_info<T>::id();
  if (type_id >= m_component_pools.count || !m_component_pools[type_id].pool)
    return nullptr;

  return static_cast<SparseEntitySet<T>*>(m_component_pools[type_id].pool);
}

template<typename ExtraComponents>
template<typename T>
bool EntityManager<ExtraComponents>::has_component_pool() {
  static_assert(component_info<T>::is_registered != false, "Unregistered Component");
  const USize type_id = component_info<T>::id();
  return type_id < m_component_pools.count && m_component_pools[type_id].pool;
}

template<typename ExtraComponents>
inline Entity EntityManager<ExtraComponents>::create() {
  const uint32_t idx =
      m_free_list.count == 0 ? static_cast<U32>(m_generations.count) + 1 : pop_free_list();

  if (idx >= m_generations.count)
    m_generations.push(m_pool_arena, 0);

  Entity e = {idx, m_generations[idx]};

  m_entity_masks.arena = m_pool_arena;
  m_entity_masks.add(e, ComponentMask{});
  return e;
}

template<typename ExtraComponents>
inline void EntityManager<ExtraComponents>::destroy(const Entity e) {
  if (!is_alive(e))
    return;

  ComponentMask* mask = m_entity_masks.get(e);
  assert(mask);

  for (U64 i = 0; i < m_component_pools.count; ++i) {
    auto& node = m_component_pools[i];
    if (node.pool && mask->test(i))
      node.remove_fn(node.pool, e);
  }
  mask->reset();
  m_generations[e.index]++;
  m_free_list.push(m_pool_arena, e.index);
}

template<typename ExtraComponents>
inline bool EntityManager<ExtraComponents>::is_alive(const Entity e) const {
  return e.index < m_generations.count && m_generations[e.index] == e.generation;
}

template<typename ExtraComponents>
inline uint32_t EntityManager<ExtraComponents>::pop_free_list() {
  const auto idx = m_free_list[m_free_list.count - 1];
  m_free_list.count--;
  return idx;
}

template<typename ExtraComponents>
inline void EntityManager<ExtraComponents>::serialize(Serializer& s) const {
  s.write(static_cast<U32>(m_generations.count));
  for (U64 i = 0; i < m_generations.count; ++i)
    s.write(m_generations[i]);

  s.write(static_cast<U32>(m_free_list.count));
  for (U64 i = 0; i < m_free_list.count; ++i)
    s.write(m_free_list[i]);

  const auto& mask_entities = m_entity_masks.get_dense_entities();
  U32         aliveCount    = 0;
  for (U64 i = 0; i < mask_entities.count; ++i) {
    if (is_alive(mask_entities[i]))
      aliveCount++;
  }
  s.write(aliveCount);

  for (U64 i = 0; i < mask_entities.count; ++i) {
    Entity e = mask_entities[i];
    if (!is_alive(e))
      continue;

    s.write(e.index);
    s.write(e.generation);
    const ComponentMask* mask = m_entity_masks.get(e);
    if (mask) {
      for (USize word = 0; word < 4; ++word) {
        U64 packed = 0;
        for (USize bit = 0; bit < 64; ++bit) {
          if (mask->test(word * 64 + bit))
            packed |= (U64(1) << bit);
        }
        s.write(packed);
      }
    } else {
      for (USize i = 0; i < 4; ++i)
        s.write(static_cast<U64>(0));
    }
  }

  U32 serializable_count = 0;
  for (U64 i = 0; i < m_component_pools.count; ++i) {
    if (m_component_pools[i].pool && sd::ComponentFactory::is_registered(i))
      serializable_count++;
  }
  s.write(serializable_count);

  for (U64 i = 0; i < m_component_pools.count; ++i) {
    if (m_component_pools[i].pool && sd::ComponentFactory::is_registered(i)) {
      s.write(static_cast<U32>(i));
      m_component_pools[i].serialize_fn(m_component_pools[i].pool, s);
    }
  }
}

template<typename ExtraComponents>
inline void EntityManager<ExtraComponents>::deserialize(Serializer& s) {
  {
    U32 count = s.read<U32>();
    for (U32 i = 0; i < count; ++i)
      m_generations.push(m_pool_arena, s.read<U32>());
  }

  {
    U32 count = s.read<U32>();
    for (U32 i = 0; i < count; ++i)
      m_free_list.push(m_pool_arena, s.read<U32>());
  }

  U32 maskCount        = s.read<U32>();
  m_entity_masks.arena = m_pool_arena;
  for (U32 i = 0; i < maskCount; ++i) {
    U32    index      = s.read<U32>();
    U32    generation = s.read<U32>();
    Entity e{index, generation};
    m_entity_masks.add(e, ComponentMask{});
    ComponentMask* mask = m_entity_masks.get(e);
    if (mask) {
      for (USize word = 0; word < 4; ++word) {
        U64 packed = s.read<U64>();
        for (USize bit = 0; bit < 64; ++bit) {
          if (packed & (static_cast<U64>(1) << bit))
            mask->set(word * 64 + bit);
        }
      }
    } else {
      for (USize j = 0; j < 4; ++j)
        s.read<U64>();
    }
  }

  for (U32 i = 0; i < m_free_list.count; ++i) {
    assert(m_generations[m_free_list[i]] > 0 && "Freelist index has generation 0");
    Entity e{m_free_list[i], m_generations[m_free_list[i]] - 1};
    m_entity_masks.add(e, ComponentMask{});
  }

  U32 serializable_count = s.read<U32>();
  m_component_pools.clear();

  for (U32 i = 0; i < serializable_count; ++i) {
    U32  componentId = s.read<U32>();
    auto entry       = sd::ComponentFactory::create(componentId, m_pool_arena);
    if (entry.pool) {
      entry.deserialize_fn(entry.pool, s);
      if (componentId >= m_component_pools.count)
        m_component_pools.push(m_pool_arena, ComponentPoolNode{});
      m_component_pools[componentId] = entry;
    }
  }
}

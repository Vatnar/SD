#pragma once
#include <bitset>
#include <vector>

#include "ComponentFactory.hpp"
#include "ComponentPoolNode.hpp"
#include "Entity.hpp"
#include "SD/arena.hpp"
#include "SD/core/logging.hpp"
#include "SparseEntitySet.hpp"
#include "component_registration.hpp"
#include "components.hpp"

namespace sd {

template<typename ExtraComponents>
struct EntityManager;

template<typename ExtraComponents = ComponentGroup<>, typename... Components>
struct ViewImpl {
  using manager_type   = EntityManager<ExtraComponents>;
  using all_components = ConcatComponentGroups_t<components::EngineComponents, ExtraComponents>;

  manager_type&           m_manager;
  const ArenaVec<Entity>* m_smallest_pool = nullptr;

  explicit ViewImpl(manager_type& manager);

  struct Iterator {
    manager_type&           manager;
    const ArenaVec<Entity>* entities;
    USize                   index;

    using iterator_category = std::forward_iterator_tag;
    using value_type        = std::tuple<Entity, Components&...>;
    using difference_type   = std::ptrdiff_t;
    using pointer           = void;
    using reference         = value_type;

    Iterator(manager_type& em, const ArenaVec<Entity>* dense_entities, USize idx);
    Iterator& operator++();

    bool operator==(const Iterator& other) const {
      return index == other.index && entities == other.entities;
    }

    bool operator!=(const Iterator& other) const { return !(*this == other); }

    std::tuple<Entity, Components&...> operator*() const;

    void next();

    [[nodiscard]] bool is_valid() const;
  };

  Iterator begin();
  Iterator end();

  template<typename Component>
  void check_size(USize& minSize);
};

template<typename ExtraComponents>
struct EntityManager {
  using all_components = ConcatComponentGroups_t<components::EngineComponents, ExtraComponents>;

  template<typename T>
  using component_info = ComponentTraits<T, all_components>;

  static_assert(IsUniqueComponentGroup<all_components>::value,
                "Duplicate component type in ECS schema");
  Entity create();

  template<typename T, typename... Args>
  T* add_component(Entity e, Args&&... args);

  template<typename T>
  T* try_get_component(Entity e);

  template<typename T>
  T& get_component(Entity e);

  template<typename T>
  const T& get_component(Entity e) const;

  void serialize(Serializer& s) const;
  void deserialize(Serializer& s);

  template<typename T>
  bool try_remove_component(Entity e);

  void destroy(Entity e);

  void clear() {
    m_component_pools.clear();
    m_entity_masks.clear();
    m_generations.clear();
    m_free_list.clear();
    if (m_pool_arena)
      m_pool_arena->clear();
  }

  [[nodiscard]] bool is_alive(Entity e) const;
  [[nodiscard]] int  get_entity_count() const { return m_entity_masks.size(); }
  [[nodiscard]] int  get_alive_entity_count() const {
    int alive = 0;
    for (U64 i = 0; i < m_entity_masks.get_dense_entities().count; ++i) {
      Entity e = m_entity_masks.get_dense_entities()[i];
      if (e.index < m_generations.count && is_alive(e))
        ++alive;
    }
    return alive;
  }

  template<typename T>
  struct UnpackGroup {
    static constexpr bool is_group = false;
  };

  template<typename... Ts>
  struct UnpackGroup<std::tuple<Ts...>> {
    using type                     = ViewImpl<ExtraComponents, Ts...>;
    static constexpr bool is_group = true;
  };

  template<typename... Ts>
  struct UnpackGroup<ComponentGroup<Ts...>> {
    using type                     = ViewImpl<ExtraComponents, Ts...>;
    static constexpr bool is_group = true;
  };

  template<typename... Args>
  auto view();

  template<typename T>
  [[nodiscard]] bool has_component(Entity e) const;

  template<typename... Components>
  std::tuple<Components&...> get_component_group(Entity e);

  template<typename... Components>
  std::tuple<const Components&...> get_component_group(Entity e) const;

  template<typename T>
  SparseEntitySet<T>* get_component_pool();

  template<typename T>
  bool has_component_pool();

  Arena* m_pool_arena = nullptr;
  U32    pop_free_list();

  ArenaVec<U32> m_generations;
  ArenaVec<U32> m_free_list;

  ArenaVec<ComponentPoolNode> m_component_pools;

  using ComponentMask = std::bitset<256>;
  SparseEntitySet<ComponentMask> m_entity_masks;

  friend class RuntimeStateManager;
};

#include "impl/EntityManager.inl"
} // namespace sd

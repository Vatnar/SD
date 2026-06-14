// TODO(docs): Add file-level Doxygen header
//   - @file EntityManager.hpp
//   - @brief Entity-Component System core manager
//   - Entity creation, destruction, and component management
//   - View system for component queries
#pragma once
#include <bitset>
#include <memory>
#include <vector>

#include "ComponentFactory.hpp"
#include "Entity.hpp"
#include "SD/core/logging.hpp"
#include "SparseEntitySet.hpp"
#include "component_registration.hpp"
#include "components.hpp"


namespace sd {


// TODO(docs): Document ViewImpl class
//   - Purpose: Iterator for entities with specific components
//   - Iteration pattern and validity checking
//   - begin()/end() semantics
//   - Example: Iterating over entities with Transform and Velocity


template<typename ExtraComponents>
class EntityManager;

template<typename ExtraComponents = ComponentGroup<>, typename... Components>
class ViewImpl {
  using manager_type   = EntityManager<ExtraComponents>;
  using all_components = ConcatComponentGroups_t<components::EngineComponents, ExtraComponents>;

  manager_type&              m_manager;
  const std::vector<Entity>* m_smallest_pool = nullptr;

public:
  explicit ViewImpl(manager_type& manager);
  struct Iterator {
    manager_type&              manager;
    const std::vector<Entity>* entities;
    USize                      index;

    using iterator_category = std::forward_iterator_tag;
    using value_type        = std::tuple<Entity, Components&...>;
    using difference_type   = std::ptrdiff_t;
    using pointer           = void;
    using reference         = value_type;

    Iterator(manager_type& em, const std::vector<Entity>* dense_entities, USize idx);
    Iterator& operator++();

    bool operator==(const Iterator& other) const {
      return index == other.index && entities == other.entities;
    }

    bool operator!=(const Iterator& other) const { return !(*this == other); }

    std::tuple<Entity, Components&...> operator*() const;

  private:
    void next();

    [[nodiscard]] bool is_valid() const;
  };

  Iterator begin();

  Iterator end();


private:
  template<typename Component>
  void check_size(USize& minSize);
};


// TODO: make multi-threaded safe.
/**
 * EntityManager is the central ECS manager, that manages entities, components and allows for
 * queries into the system using views. If you intend to use the entitymanager with a custom
 * component, it will require you to register the component using the \ref REGISTER_COMPONENT macro.
 *
 * # Querying the ECS
 * Many systems and subsystems query the ECS for entities and their components, the views can be
 * created per component, for multiple components, or of a ComponentGroup of components.
 *
 */
template<typename ExtraComponents>
class EntityManager : public Serializable {
  using all_components = ConcatComponentGroups_t<components::EngineComponents, ExtraComponents>;
  template<typename T>
  using component_info = ComponentTraits<T, all_components>;

  static_assert(IsUniqueComponentGroup<all_components>::value,
                "Duplicate component type in ECS schema");

public:
  Entity create();

  template<typename T, typename... Args>
  T* add_component(Entity e, Args&&... args);

  template<typename T>
  T* try_get_component(Entity e);

  /**
   * @brief See TryGetComponent for safe pointer version
   * @tparam T
   * @param e
   * @return
   */
  template<typename T>
  T& get_component(Entity e);
  template<typename T>
  const T& get_component(Entity e) const;
  void     serialize(Serializer& s) const override;
  void     deserialize(Serializer& s) override;
  template<typename T>
  bool try_remove_component(Entity e);

  void destroy(Entity e);
  void clear() {
    m_generations.clear();
    m_free_list.clear();
    m_component_pools.clear();
    m_entity_masks.clear();
  }

  // [[nodiscard]] std::vector<ComponentDebugInfo> get_all_component_info(Entity e) const;

  [[nodiscard]] bool is_alive(Entity e) const;
  [[nodiscard]] int  get_entity_count() const { return m_entity_masks.size(); }
  [[nodiscard]] int  get_alive_entity_count() const {
    int alive = 0;
    for (Entity e : m_entity_masks.get_dense_entities()) {
      if (is_alive(e))
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


private:
  U32              pop_free_list();
  std::vector<U32> m_generations;
  std::vector<U32> m_free_list;

  std::vector<std::unique_ptr<SparseEntitySetBase>> m_component_pools;
  // NOTE: Hard capped at 256 for now, shouldn't really be a problem. Change in future if needed
  using ComponentMask = std::bitset<256>;
  SparseEntitySet<ComponentMask> m_entity_masks;

  // INVARIANTS:
  // 1. For alive entity e: m_generations[e.index] == e.generation
  // 2. mFreeList contains only indices of destroyed entities (not in use)
  // 3. m_entity_masks has entry for every index in m_generations
  // 4. Component mask bit is set iff component exists in corresponding pool

#ifdef SD_DEBUG
  void ValidateInvariants() const {
    assert(m_entity_masks.size() == m_generations.size());
    for (U32 idx : m_free_list) {
      assert(idx < m_generations.size());
    }
  }
#endif

  friend class RuntimeStateManager;
};

#include "impl/EntityManager.inl"
} // namespace sd

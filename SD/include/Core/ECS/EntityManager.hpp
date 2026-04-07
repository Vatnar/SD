// TODO(docs): Add file-level Doxygen header
//   - @file EntityManager.hpp
//   - @brief Entity-Component System core manager
//   - Entity creation, destruction, and component management
//   - View system for component queries
#pragma once
#include <bitset>
#include <memory>
#include <vector>

#include "Component.hpp"
#include "ComponentFactory.hpp"
#include "Entity.hpp"
#include "EntityManager.hpp"
#include "SparseEntitySet.hpp"


namespace SD {
class EntityManager;

// TODO(docs): Document ViewImpl class
//   - Purpose: Iterator for entities with specific components
//   - Iteration pattern and validity checking
//   - begin()/end() semantics
//   - Example: Iterating over entities with Transform and Velocity
template<typename... Components>
class ViewImpl {
  EntityManager& mManager;
  const std::vector<Entity>* mSmallestPool = nullptr;

public:
  explicit ViewImpl(EntityManager& manager);
  struct Iterator {
    EntityManager& manager;
    const std::vector<Entity>* entities;
    usize index;

    using iterator_category = std::forward_iterator_tag;
    using value_type = std::tuple<Entity, Components&...>;
    using difference_type = std::ptrdiff_t;
    using pointer = void;
    using reference = value_type;

    Iterator(EntityManager& em, const std::vector<Entity>* denseEntities, usize idx);
    Iterator& operator++();

    bool operator==(const Iterator& other) const {
      return index == other.index && entities == other.entities;
    }

    bool operator!=(const Iterator& other) const { return !(*this == other); }

    std::tuple<Entity, Components&...> operator*() const;

  private:
    void Next();

    [[nodiscard]] bool IsValid() const;
  };

  Iterator begin();

  Iterator end();


private:
  template<typename Component>
  void CheckSize(usize& minSize);
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
class EntityManager : public Serializable {
public:
  Entity Create();

  template<typename T, typename... Args>
  T* AddComponent(Entity e, Args&&... args);

  template<typename T>
  T* TryGetComponent(Entity e);

  /**
   * @brief See TryGetComponent for safe pointer version
   * @tparam T
   * @param e
   * @return
   */
  template<typename T>
  T& GetComponent(Entity e);
  void Serialize(Serializer& s) const override;
  void Deserialize(Serializer& s) override;
  template<typename T>
  bool TryRemoveComponent(Entity e);

  void Destroy(Entity e);
  [[nodiscard]] std::vector<ComponentDebugInfo> GetAllComponentInfo(Entity e) const;

  [[nodiscard]] bool IsAlive(Entity e) const;
  [[nodiscard]] int GetEntityCount() const { return mEntityMasks.Size(); }


  template<typename T>
  struct UnpackGroup {
    static constexpr bool IsGroup = false;
  };
  template<typename... Ts>
  struct UnpackGroup<std::tuple<Ts...>> {
    using type = ViewImpl<Ts...>;
    static constexpr bool IsGroup = true;
  };
  template<typename... Ts>
  struct UnpackGroup<ComponentGroup<Ts...>> {
    using type = ViewImpl<Ts...>;
    static constexpr bool IsGroup = true;
  };

  template<typename... Args>
  auto View();

  template<typename T>
  [[nodiscard]] bool HasComponent(Entity e) const;

  template<typename... Components>
  std::tuple<Components&...> GetComponentGroup(Entity e);

  template<typename... Components>
  std::tuple<const Components&...> GetComponentGroup(Entity e) const;

  template<typename T>
  SparseEntitySet<T>* GetComponentPool();

  template<typename T>
  bool HasComponentPool();


private:
  u32 PopFreeList();
  std::vector<u32> mGenerations;
  std::vector<u32> mFreeList;

  std::vector<std::unique_ptr<SparseEntitySetBase>> mComponentPools;
  // NOTE: Hard capped at 256 for now, shouldn't really be a problem. Change in future if needed
  using ComponentMask = std::bitset<256>;
  SparseEntitySet<ComponentMask> mEntityMasks;

  // INVARIANTS:
  // 1. For alive entity e: mGenerations[e.index] == e.generation
  // 2. mFreeList contains only indices of destroyed entities (not in use)
  // 3. mEntityMasks has entry for every index in mGenerations
  // 4. Component mask bit is set iff component exists in corresponding pool
  //
  // TODO(invariants): Add ValidateInvariants() debug function:
  //   #ifndef NDEBUG
  //   void ValidateInvariants() const {
  //     assert(mEntityMasks.Size() == mGenerations.size());
  //     for (u32 idx : mFreeList) {
  //       assert(idx < mGenerations.size());
  //     }
  //   }
  //   #endif
  // Call at end of Create(), Destroy() in debug builds.

  friend class RuntimeStateManager;
};

#include "impl/EntityManager.inl"
} // namespace SD

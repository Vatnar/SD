#pragma once
#include <bitset>
#include <memory>
#include <vector>

#include "Component.hpp"
#include "Entity.hpp"
#include "EntityManager.hpp"
#include "SparseEntitySet.hpp"


namespace SD {
class EntityManager;
template<typename... Components>
class ViewImpl {
  EntityManager& mManager;
  const std::vector<Entity>* mSmallestPool = nullptr;

public:
  explicit ViewImpl(EntityManager& manager);
  struct Iterator {
    EntityManager& manager;
    const std::vector<Entity>* entities;
    size_t index;

    using iterator_category = std::forward_iterator_tag;
    using value_type = std::tuple<Entity, Components&...>;
    using difference_type = std::ptrdiff_t;
    using pointer = void;
    using reference = value_type;

    Iterator(EntityManager& em, const std::vector<Entity>* denseEntities, size_t idx);
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
  void CheckSize(size_t& minSize);
};


class EntityManager {
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

  template<typename T>
  bool TryRemoveComponent(Entity e);

  void Destroy(Entity e);
  [[nodiscard]] std::vector<ComponentDebugInfo> GetAllComponentInfo(Entity e) const;

  [[nodiscard]] bool IsAlive(Entity e) const;


  template<typename T>
  struct UnpackGroup {
    using type = T;
  };
  template<typename... Ts>
  struct UnpackGroup<std::tuple<Ts...>> {
    using type = ViewImpl<Ts...>;
  };
  template<typename... Ts>
  struct UnpackGroup<ComponentGroup<Ts...>> {
    using type = ViewImpl<Ts...>;
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
  uint32_t PopFreeList();
  std::vector<uint32_t> mGenerations;
  std::vector<uint32_t> mFreeList;

  std::vector<std::unique_ptr<SparseEntitySetBase>> mComponentPools;
  // TODO: make resizable
  using ComponentMask = std::bitset<64>;
  SparseEntitySet<ComponentMask> mEntityMasks;
};

#include "impl/EntityManager.inl"
} // namespace SD

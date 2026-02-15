#pragma once
#include <bit>
#include <bitset>
#include <cmath>
#include <concepts>
#include <cstdint>
#include <memory>
#include <vector>

#include "Utils/Utils.hpp"
#include "spdlog/spdlog.h"

struct Entity {
  uint32_t index;
  uint32_t generation;

  bool operator==(Entity other) const;
};
inline bool Entity::operator==(const Entity other) const {
  return index == other.index && generation == other.generation;
}


template<typename T>

struct ComponentTraits {
  static constexpr const char* Name = "Unknown";
};
#define REGISTER_SD_COMPONENT(Type)                  \
  template<>                                         \
  struct ComponentTraits<Type> {                     \
    static constexpr const char* Name = "SD_" #Type; \
  }

#define REGISTER_COMPONENT(Type)               \
  template<>                                   \
  struct ComponentTraits<Type> {               \
    static constexpr const char* Name = #Type; \
  };

consteval size_t log2_int(std::unsigned_integral auto n) {
  // log2(n) = bit_width(n) - 1
  return n == 0 ? 0 : std::bit_width(n) - 1;
}

struct ComponentDebugInfo {
  const char* name;
  void* data;
};

class SparseEntitySetBase {
public:
  virtual ~SparseEntitySetBase() = default;
  virtual bool Remove(Entity entity) = 0;

  virtual std::optional<ComponentDebugInfo> GetDebugInfo(Entity e) = 0;
};

/**
 * Sparse -> Dense set for enitities.
 * @tparam T Data associated, for instance component structs
 */
template<typename T>
class SparseEntitySet : public SparseEntitySetBase {
  static constexpr size_t PAGE_SIZE = 1024;
  static constexpr size_t SHIFT = log2_int(PAGE_SIZE);
  static constexpr size_t MASK = PAGE_SIZE - 1;

  std::vector<std::unique_ptr<size_t[]>> sparse;
  std::vector<T> denseData;
  std::vector<Entity> denseEntities;

public:
  // TODO: use << and &, instead of / and %
  template<typename... Args>
  void Add(Entity entity, Args&&... args) {
    const size_t page = entity.index >> SHIFT;
    const size_t offset = entity.index & MASK;

    if (page >= sparse.size())
      sparse.resize(page + 1);
    if (!sparse[page]) {
      sparse[page] = std::make_unique<size_t[]>(PAGE_SIZE);
      std::fill_n(sparse[page].get(), PAGE_SIZE, std::numeric_limits<size_t>::max());
    }

    sparse[page][offset] = denseEntities.size(); // end
    denseData.push_back(T{std::forward<Args>(args)...});
    denseEntities.push_back(entity);
  }

  // TODO: Test
  /**
   *
   * @param entity
   * @return true if successfully removed, false, if it doesnt exist
   */
  bool Remove(Entity entity) override {
    const size_t page = entity.index >> SHIFT;
    const size_t offset = entity.index & MASK;

    if (page >= sparse.size() || !sparse[page])
      return false; // out of bounds

    size_t denseIdx = sparse[page][offset];
    if (denseIdx == std::numeric_limits<size_t>::max())
      return false; // doesnt exist

    if (denseEntities[denseIdx] != entity)
      return false; // wrong generation

    // move, last to removed pos
    size_t lastIdx = denseEntities.size() - 1;
    Entity lastEntity = denseEntities[lastIdx];

    denseData[denseIdx] = std::move(denseData[lastIdx]);
    denseEntities[denseIdx] = lastEntity;

    // update sparse index
    const size_t lastPage = lastEntity.index >> SHIFT;
    const size_t lastOffset = lastEntity.index & MASK;
    sparse[lastPage][lastOffset] = denseIdx;

    // free index
    sparse[page][offset] = std::numeric_limits<size_t>::max();

    // remove duplicated last
    denseData.pop_back();
    denseEntities.pop_back();


    return true;
  }

  /**
   * Retrieve data for this entity
   * @param entity
   * @return Data associated for this entity, or nullptr if it doesn't exist
   */
  T* Get(Entity entity) {
    const size_t page = entity.index >> SHIFT;
    const size_t offset = entity.index & MASK;

    if (page >= sparse.size() || !sparse[page])
      return nullptr;

    size_t denseIdx = sparse[page][offset];

    if (denseIdx == std::numeric_limits<size_t>::max())
      return nullptr; // cant get something that doesnt have data in this component.

    // Check if the entity is still alive
    if (denseEntities[denseIdx] != entity)
      return nullptr;


    return &denseData[denseIdx];
  }
  std::optional<ComponentDebugInfo> GetDebugInfo(Entity e) override {
    T* ptr = Get(e);
    if (!ptr)
      return std::nullopt;

    return ComponentDebugInfo{ComponentTraits<T>::Name, ptr};
  }

  T* operator[](const Entity idx) { return Get(idx); }
};

// NOTE: If i use shared libs, i need to make sure this is exported
struct ComponentIdGenerator {
  static inline size_t counter = 0;

  template<typename T>
  static size_t GetComponentTypeID() {
    static const size_t id = counter++;
    return id;
  }
};


class EntityManager {
public:
  Entity Create() {
    const uint32_t idx = mFreeList.empty() ? mGenerations.size() : PopFreeList();

    if (idx >= mGenerations.size()) {
      mGenerations.resize(idx + 1, 0);
    }
    Entity e = {idx, mGenerations[idx]};

    if (!mEntityMasks.Get(e))
      mEntityMasks.Add(e, ComponentMask{});
    else
      mEntityMasks.Get(e)->reset();

    return e;
  }

  template<typename T, typename... Args>
  T* AddComponent(Entity e, Args&&... args) {
    size_t typeId = ComponentIdGenerator::GetComponentTypeID<T>();

    if (typeId >= mComponentPools.size())
      mComponentPools.resize(typeId + 1);
    if (!mComponentPools[typeId])
      mComponentPools[typeId] = std::make_unique<SparseEntitySet<T>>();

    // add data
    auto* pool = static_cast<SparseEntitySet<T>*>(mComponentPools[typeId].get());
    pool->Add(e, std::forward<Args>(args)...);

    mEntityMasks[e]->set(typeId);
    return pool->Get(e);
  }

  template<typename T>
  T* TryGetComponent(Entity e) {
    size_t typeId = ComponentIdGenerator::GetComponentTypeID<T>();
    if (!mComponentPools[typeId])
      return nullptr;
    if (!mEntityMasks[e]->test(typeId))
      return nullptr;
    auto* pool = static_cast<SparseEntitySet<T>*>(mComponentPools[typeId].get());
    return pool->Get(e);
  }

  void Destroy(const Entity e) {
    if (!IsAlive(e))
      return;


    // TODO: Ranges?
    ComponentMask* mask = mEntityMasks[e];
    assert(mask);

    for (size_t i = 0; i < mask->size(); ++i) {
      if (mask->test(i)) {
        if (mComponentPools[i])
          mComponentPools[i]->Remove(e);
      }
    }
    mask->reset();
    mGenerations[e.index]++;
    mFreeList.push_back(e.index);
  }
  [[nodiscard]] std::vector<ComponentDebugInfo> GetAllComponentNames(Entity e) const {
    std::vector<ComponentDebugInfo> components;
    if (!IsAlive(e))
      return {};
    for (auto& pool : mComponentPools) {
      if (!pool)
        continue;

      if (auto info = pool->GetDebugInfo(e)) {
        components.push_back(info.value());
      }
    }
    return components;
  }

  [[nodiscard]] bool IsAlive(const Entity e) const { return mGenerations[e.index] == e.generation; }

private:
  uint32_t PopFreeList() {
    const auto idx = mFreeList.back();
    mFreeList.pop_back();
    return idx;
  }
  std::vector<uint32_t> mGenerations;
  std::vector<uint32_t> mFreeList;

  // Components, etc
  // TODO: Set these to pointers of where the sparsenetity sets are actually stored, (in the
  //  systems)
  std::vector<std::unique_ptr<SparseEntitySetBase>> mComponentPools;
  // TODO: make resizable
  using ComponentMask = std::bitset<64>;
  SparseEntitySet<ComponentMask> mEntityMasks;

  // TODO: Storage for each component system
};

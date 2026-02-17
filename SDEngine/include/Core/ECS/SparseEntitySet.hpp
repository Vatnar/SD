#pragma once
#include "Entity.hpp"
#include "Utils/Utils.hpp"

namespace SD {
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
  static constexpr usize PAGE_SIZE = 1024;
  static constexpr usize SHIFT = SD::Math::log2_int(PAGE_SIZE);
  static constexpr usize MASK = PAGE_SIZE - 1;

  std::vector<std::unique_ptr<usize[]>> sparse;
  std::vector<T> denseData;
  std::vector<Entity> denseEntities;

public:
  // TODO: use << and &, instead of / and %
  template<typename... Args>
  void Add(Entity entity, Args&&... args);

  // TODO: Test
  /**
   *
   * @param entity
   * @return true if successfully removed, false, if it doesnt exist
   */
  bool Remove(Entity entity) override;

  /**
   * Retrieve data for this entity
   * @param entity
   * @return Data associated for this entity, or nullptr if it doesn't exist
   */
  T* Get(Entity entity);
  [[nodiscard]] const T* Get(Entity entity) const;

  std::optional<ComponentDebugInfo> GetDebugInfo(Entity e) override;
  T* operator[](const Entity idx) { return Get(idx); }
  [[nodiscard]] const std::vector<Entity>& GetDenseEntities() const { return denseEntities; }

  [[nodiscard]] usize Size() const { return denseEntities.size(); }
};

#include "impl/SparseEntitySet.inl"
} // namespace SD

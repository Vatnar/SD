#pragma once
#include "Core/Base.hpp"
#include "Entity.hpp"

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
  static constexpr usize SHIFT = Math::log2_int(PAGE_SIZE);
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

  // TODO: untested
  void SerializeTo(std::vector<char>& out) const {
    // Header: size_t entity_count, size_t sizeof(T).
    size_t entity_count = denseEntities.size();
    size_t comp_size = sizeof(T);

    out.resize(sizeof(size_t) * 2 + entity_count * (sizeof(Entity) + comp_size));
    char* ptr = out.data();
    memcpy(ptr, &entity_count, sizeof(size_t));
    ptr += sizeof(size_t);
    memcpy(ptr, &comp_size, sizeof(size_t));
    ptr += sizeof(size_t);

    // Dense entities + data.
    memcpy(ptr, denseEntities.data(), entity_count * sizeof(Entity));
    ptr += entity_count * sizeof(Entity);
    memcpy(ptr, denseData.data(), entity_count * comp_size);
  }

  // TODO: untested
  void DeserializeFrom(const std::vector<char>& data) {
    const char* ptr = data.data();
    size_t entity_count, comp_size;
    memcpy(&entity_count, ptr, sizeof(size_t));
    ptr += sizeof(size_t);
    memcpy(&comp_size, ptr, sizeof(size_t));
    ptr += sizeof(size_t);

    // Resize.
    denseEntities.resize(entity_count);
    denseData.resize(entity_count);

    // Replay.
    memcpy(denseEntities.data(), ptr, entity_count * sizeof(Entity));
    ptr += entity_count * sizeof(Entity);
    memcpy(denseData.data(), ptr, entity_count * comp_size);
  }

  friend class RuntimeStateManager;
};

#include "impl/SparseEntitySet.inl"
} // namespace SD

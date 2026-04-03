// TODO(docs): Add file-level Doxygen header
//   - @file SparseEntitySet.hpp
//   - @brief Sparse set data structure for component storage
//   - Memory layout: sparse array -> dense array -> component data
//   - Performance characteristics (O(1) add/remove/get)
#pragma once
#include "Core/Base.hpp"
#include "Entity.hpp"

namespace SD {
// TODO(docs): Document SparseEntitySetBase interface
//   - Purpose: Type-erased base for component pools
//   - Used by EntityManager for heterogeneous storage
class SparseEntitySetBase {
public:
  virtual ~SparseEntitySetBase() = default;
  virtual bool Remove(Entity entity) = 0;

  virtual std::optional<ComponentDebugInfo> GetDebugInfo(Entity e) = 0;
};

// TODO(docs): Document SparseEntitySet class thoroughly
//   - Purpose: Sparse set storing components with O(1) operations
//   - Memory layout explanation (sparse pages, dense arrays)
//   - Page size rationale (1024 entities per page)
//   - Add: O(1) amortized (with page allocation)
//   - Remove: O(1) with swap-and-pop
//   - Get: O(1)
//   - Serialization/Deserialization for state persistence
//   - Example: How EntityManager uses this for component pools
/**
 * Sparse -> Dense set for enitities.
 * @tparam T Data associated, for instance component structs
 */
template<typename T>
class SparseEntitySet : public SparseEntitySetBase {
  static constexpr usize PAGE_SIZE = 1024;
  static constexpr usize SHIFT = Math::log2_int(PAGE_SIZE);
  static constexpr usize MASK = PAGE_SIZE - 1;

  // INVARIANTS:
  // 1. denseEntities.size() == denseData.size() (parallel arrays)
  // 2. sparse[page][offset] is either valid dense index OR sentinel (max usize)
  // 3. For each i in [0, denseEntities.size()): sparse[denseEntities[i]] points back to i
  //
  // TODO(invariants): Add ValidateInvariants() debug function:
  //   #ifndef NDEBUG
  //   void ValidateInvariants() const {
  //     assert(denseEntities.size() == denseData.size());
  //     for (size_t i = 0; i < denseEntities.size(); ++i) {
  //       Entity e = denseEntities[i];
  //       usize page = e.index >> SHIFT;
  //       usize offset = e.index & MASK;
  //       assert(page < sparse.size() && sparse[page]);
  //       assert(sparse[page][offset] == i);
  //     }
  //   }
  //   #endif
  // Call at end of Add(), Remove(), and DeserializeFrom() in debug builds.

  std::vector<std::unique_ptr<usize[]>> sparse;
  std::vector<T> denseData;
  std::vector<Entity> denseEntities;

public:
  template<typename... Args>
  void Add(Entity entity, Args&&... args);

  /**
   * Remove an entity from the set
   * @param entity Entity to remove
   * @return true if successfully removed, false if it doesn't exist
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

  void DeserializeFrom(const std::vector<char>& data) {
    const char* ptr = data.data();
    size_t entity_count, comp_size;
    memcpy(&entity_count, ptr, sizeof(size_t));
    ptr += sizeof(size_t);
    memcpy(&comp_size, ptr, sizeof(size_t));
    ptr += sizeof(size_t);

    denseEntities.resize(entity_count);
    denseData.resize(entity_count);

    memcpy(denseEntities.data(), ptr, entity_count * sizeof(Entity));
    ptr += entity_count * sizeof(Entity);
    memcpy(denseData.data(), ptr, entity_count * comp_size);

    sparse.clear();
    for (size_t i = 0; i < entity_count; ++i) {
      Entity e = denseEntities[i];
      const usize page = e.index >> SHIFT;
      const usize offset = e.index & MASK;

      if (page >= sparse.size())
        sparse.resize(page + 1);
      if (!sparse[page]) {
        sparse[page] = std::make_unique<usize[]>(PAGE_SIZE);
        std::fill_n(sparse[page].get(), PAGE_SIZE, std::numeric_limits<usize>::max());
      }
      sparse[page][offset] = i;
    }
  }

  friend class RuntimeStateManager;
};

#include "impl/SparseEntitySet.inl"
} // namespace SD

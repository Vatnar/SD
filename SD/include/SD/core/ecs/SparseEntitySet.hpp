// TODO(docs): Add file-level Doxygen header
//   - @file SparseEntitySet.hpp
//   - @brief Sparse set data structure for component storage
//   - Memory layout: sparse array -> dense array -> component data
//   - Performance characteristics (O(1) add/remove/get)
#pragma once
#include "Entity.hpp"
#include "SD/core/math_utils.hpp"
#include "SD/utils/serialization.hpp"
#include "component_registration.hpp"

namespace sd {
// TODO(docs): Document SparseEntitySetBase interface
//   - Purpose: Type-erased base for component pools
//   - Used by EntityManager for heterogeneous storage
class SparseEntitySetBase : public Serializable {
public:
  ~SparseEntitySetBase() override    = default;
  virtual bool remove(Entity entity) = 0;

  // virtual std::optional<ComponentDebugInfo> get_debug_info(Entity e) = 0;
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

// TODO(vatnar): SparseEntity Set does make_unique on per page and push_back on each
// component add. which trigger a heap allocation. Rather add an arena for this, one arena
// per archetype or similar
template<typename T>
class SparseEntitySet : public SparseEntitySetBase {
  static constexpr usize PAGE_SIZE = 1024;
  static constexpr usize SHIFT     = math::log2_int(PAGE_SIZE);
  static constexpr usize MASK      = PAGE_SIZE - 1;

  // INVARIANTS:
  // 1. dense_entities.size() == dense_data.size() (parallel arrays)
  // 2. sparse[page][offset] is either valid dense index OR sentinel (max usize)
  // 3. For each i in [0, dense_entities.size()): sparse[dense_entities[i]] points back to i

#ifdef SD_DEBUG
  void ValidateInvariants() const {
    assert(dense_entities.size() == dense_data.size());
    for (size_t i = 0; i < dense_entities.size(); ++i) {
      Entity e      = dense_entities[i];
      usize  page   = e.index >> SHIFT;
      usize  offset = e.index & MASK;
      assert(page < sparse.size() && sparse[page]);
      assert(sparse[page][offset] == i);
    }
  }
#endif

  std::vector<std::unique_ptr<usize[]>> sparse;
  std::vector<T>                        dense_data;
  std::vector<Entity>                   dense_entities;

public:
  template<typename... Args>
  void add(Entity entity, Args&&... args);

  /**
   * Remove an entity from the set
   * @param entity Entity to remove
   * @return true if successfully removed, false if it doesn't exist
   */
  bool remove(Entity entity) override;

  /**
   * Retrieve data for this entity
   * @param entity
   * @return Data associated for this entity, or nullptr if it doesn't exist
   */
  T*                     get(Entity entity);
  [[nodiscard]] const T* get(Entity entity) const;

  // std::optional<ComponentDebugInfo>        get_debug_info(Entity e) override;
  T*                                       operator[](const Entity idx) { return get(idx); }
  [[nodiscard]] const std::vector<Entity>& get_dense_entities() const { return dense_entities; }

  [[nodiscard]] usize size() const { return dense_entities.size(); }

  void clear() {
    sparse.clear();
    dense_data.clear();
    dense_entities.clear();
  }

  void serialize_to(std::vector<char>& out) const {
    // Header: size_t entity_count, size_t sizeof(T).
    size_t entity_count = dense_entities.size();
    size_t comp_size    = sizeof(T);

    out.resize(sizeof(size_t) * 2 + entity_count * (sizeof(Entity) + comp_size));
    char* ptr = out.data();
    memcpy(ptr, &entity_count, sizeof(size_t));
    ptr += sizeof(size_t);
    memcpy(ptr, &comp_size, sizeof(size_t));
    ptr += sizeof(size_t);

    // Dense entities + data.
    memcpy(ptr, dense_entities.data(), entity_count * sizeof(Entity));
    ptr += entity_count * sizeof(Entity);
    memcpy(ptr, dense_data.data(), entity_count * comp_size);
  }

  void deserialize_from(const std::vector<char>& data) {
    const char* ptr = data.data();
    size_t      entity_count, comp_size;
    memcpy(&entity_count, ptr, sizeof(size_t));
    ptr += sizeof(size_t);
    memcpy(&comp_size, ptr, sizeof(size_t));
    ptr += sizeof(size_t);

    dense_entities.resize(entity_count);
    dense_data.resize(entity_count);

    memcpy(dense_entities.data(), ptr, entity_count * sizeof(Entity));
    ptr += entity_count * sizeof(Entity);
    memcpy(dense_data.data(), ptr, entity_count * comp_size);

    sparse.clear();
    for (size_t i = 0; i < entity_count; ++i) {
      Entity      e      = dense_entities[i];
      const usize page   = e.index >> SHIFT;
      const usize offset = e.index & MASK;

      if (page >= sparse.size())
        sparse.resize(page + 1);
      if (!sparse[page]) {
        sparse[page] = std::make_unique<usize[]>(PAGE_SIZE); // TODO: here
        std::fill_n(sparse[page].get(), PAGE_SIZE, std::numeric_limits<usize>::max());
      }
      sparse[page][offset] = i;
    }
#ifdef SD_DEBUG
    ValidateInvariants();
#endif
  }

  void serialize(Serializer& s) const override {
    s.write(static_cast<u32>(dense_entities.size()));
    for (const auto& e : dense_entities) {
      s.write(e.index);
      s.write(e.generation);
    }
    // Only serialize component data if it's serializable
    if constexpr (SerializableComponent<T>) {
      for (const auto& data : dense_data) {
        ComponentSerializer<T>::serialize(data, s);
      }
    }
  }

  void deserialize(Serializer& s) override {
    u32 count = s.read<u32>();
    dense_entities.resize(count);
    for (u32 i = 0; i < count; ++i) {
      dense_entities[i].index      = s.read<u32>();
      dense_entities[i].generation = s.read<u32>();
    }
    dense_data.resize(dense_entities.size());
    if constexpr (SerializableComponent<T>) {
      for (auto& data : dense_data) {
        ComponentSerializer<T>::deserialize(data, s);
      }
    }
    // Rebuild sparse index
    sparse.clear();
    for (size_t i = 0; i < dense_entities.size(); ++i) {
      Entity      e      = dense_entities[i];
      const usize page   = e.index >> SHIFT;
      const usize offset = e.index & MASK;

      if (page >= sparse.size())
        sparse.resize(page + 1);
      if (!sparse[page]) {
        // TODO: here
        sparse[page] = std::make_unique<usize[]>(PAGE_SIZE);
        std::fill_n(sparse[page].get(), PAGE_SIZE, std::numeric_limits<usize>::max());
      }
      sparse[page][offset] = i;
    }
  }

  friend class RuntimeStateManager;
};

#include "impl/SparseEntitySet.inl"
} // namespace sd

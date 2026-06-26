#pragma once
#include "Entity.hpp"
#include "SD/arena.hpp"
#include "SD/core/arena_vec.hpp"
#include "SD/core/math_utils.hpp"
#include "SD/utils/serialization.hpp"
#include "component_registration.hpp"

namespace sd {

template<typename T>
struct SparseEntitySet {
  static constexpr USize PAGE_SIZE = 1024;
  static constexpr USize SHIFT     = math::log2_int(PAGE_SIZE);
  static constexpr USize MASK      = PAGE_SIZE - 1;

  Arena* arena = nullptr;

  USize** sparse_pages = nullptr;
  U64     sparse_count = 0;
  U64     sparse_cap   = 0;

  ArenaVec<T>      dense_data;
  ArenaVec<Entity> dense_entities;

  void ensure_page(USize page) {
    if (page >= sparse_cap) {
      U64 new_cap = sparse_cap ? sparse_cap * 2 : 4;
      while (new_cap <= page)
        new_cap *= 2;
      USize** new_pages = arena->push_array<USize*>(new_cap);
      for (U64 i = 0; i < sparse_count; ++i)
        new_pages[i] = sparse_pages[i];
      for (U64 i = sparse_count; i < new_cap; ++i)
        new_pages[i] = nullptr;
      sparse_pages = new_pages;
      sparse_cap   = new_cap;
    }
    if (page >= sparse_count)
      sparse_count = page + 1;
    if (!sparse_pages[page]) {
      sparse_pages[page] = arena->push_array<USize>(PAGE_SIZE);
      for (USize i = 0; i < PAGE_SIZE; ++i)
        sparse_pages[page][i] = std::numeric_limits<USize>::max();
    }
  }

  template<typename... Args>
  void add(Entity entity, Args&&... args) {
    USize page   = entity.index >> SHIFT;
    USize offset = entity.index & MASK;

    ensure_page(page);

    USize dense_idx = sparse_pages[page][offset];
    if (dense_idx != std::numeric_limits<USize>::max()) {
      dense_data[dense_idx]     = T{std::forward<Args>(args)...};
      dense_entities[dense_idx] = entity;
    } else {
      sparse_pages[page][offset] = static_cast<USize>(dense_entities.count);
      dense_data.push(arena, T{std::forward<Args>(args)...});
      dense_entities.push(arena, entity);
    }
  }

  bool remove(Entity entity) {
    USize page   = entity.index >> SHIFT;
    USize offset = entity.index & MASK;

    if (page >= sparse_count || !sparse_pages[page])
      return false;

    USize dense_idx = sparse_pages[page][offset];
    if (dense_idx == std::numeric_limits<USize>::max())
      return false;
    if (dense_entities[dense_idx] != entity)
      return false;

    U64    last_idx    = dense_entities.count - 1;
    Entity last_entity = dense_entities[last_idx];

    dense_data[dense_idx]     = dense_data[last_idx];
    dense_entities[dense_idx] = last_entity;

    USize last_page                      = last_entity.index >> SHIFT;
    USize last_offset                    = last_entity.index & MASK;
    sparse_pages[last_page][last_offset] = static_cast<USize>(dense_idx);

    sparse_pages[page][offset] = std::numeric_limits<USize>::max();

    dense_data.count--;
    dense_entities.count--;
    return true;
  }

  T* get(Entity entity) {
    USize page   = entity.index >> SHIFT;
    USize offset = entity.index & MASK;

    if (page >= sparse_count || !sparse_pages[page])
      return nullptr;

    USize dense_idx = sparse_pages[page][offset];
    if (dense_idx == std::numeric_limits<USize>::max())
      return nullptr;
    if (dense_entities[dense_idx] != entity)
      return nullptr;

    return &dense_data[dense_idx];
  }

  const T* get(Entity entity) const {
    USize page   = entity.index >> SHIFT;
    USize offset = entity.index & MASK;

    if (page >= sparse_count || !sparse_pages[page])
      return nullptr;

    USize dense_idx = sparse_pages[page][offset];
    if (dense_idx == std::numeric_limits<USize>::max())
      return nullptr;
    if (dense_entities[dense_idx] != entity)
      return nullptr;

    return &dense_data[dense_idx];
  }

  T* operator[](Entity idx) { return get(idx); }

  const ArenaVec<Entity>& get_dense_entities() const { return dense_entities; }

  USize size() const { return static_cast<USize>(dense_entities.count); }

  void clear() {
    sparse_pages = nullptr;
    sparse_count = 0;
    sparse_cap   = 0;
    dense_data.clear();
    dense_entities.clear();
  }

  void serialize_to(std::vector<char>& out) const {
    size_t entity_count = static_cast<size_t>(dense_entities.count);
    size_t comp_size    = sizeof(T);

    out.resize(sizeof(size_t) * 2 + entity_count * (sizeof(Entity) + comp_size));
    char* ptr = out.data();
    memcpy(ptr, &entity_count, sizeof(size_t));
    ptr += sizeof(size_t);
    memcpy(ptr, &comp_size, sizeof(size_t));
    ptr += sizeof(size_t);

    memcpy(ptr, dense_entities.data, entity_count * sizeof(Entity));
    ptr += entity_count * sizeof(Entity);
    memcpy(ptr, dense_data.data, entity_count * comp_size);
  }

  void deserialize_from(const std::vector<char>& data) {
    const char* ptr = data.data();
    size_t      entity_count, comp_size;
    memcpy(&entity_count, ptr, sizeof(size_t));
    ptr += sizeof(size_t);
    memcpy(&comp_size, ptr, sizeof(size_t));
    ptr += sizeof(size_t);

    for (size_t i = 0; i < entity_count; ++i) {
      Entity e;
      memcpy(&e, ptr + i * sizeof(Entity), sizeof(Entity));
      ptr += sizeof(Entity);
    }
    ptr = data.data() + sizeof(size_t) * 2;
    for (size_t i = 0; i < entity_count; ++i) {
      Entity e;
      memcpy(&e, ptr, sizeof(Entity));
      ptr += sizeof(Entity);
      dense_entities.push(arena, e);
      T comp;
      memcpy(&comp, ptr, comp_size);
      ptr += comp_size;
      add(e, std::move(comp));
    }
  }

  void serialize(Serializer& s) const {
    s.write(static_cast<U32>(dense_entities.count));
    for (U64 i = 0; i < dense_entities.count; ++i) {
      s.write(dense_entities[i].index);
      s.write(dense_entities[i].generation);
    }
    if constexpr (SerializableComponent<T>) {
      for (U64 i = 0; i < dense_data.count; ++i) {
        ComponentSerializer<T>::serialize(dense_data[i], s);
      }
    }
  }

  void deserialize(Serializer& s) {
    U32 count = s.read<U32>();
    for (U32 i = 0; i < count; ++i) {
      Entity e;
      e.index      = s.read<U32>();
      e.generation = s.read<U32>();
      dense_entities.push(arena, e);
    }
    if constexpr (SerializableComponent<T>) {
      for (U32 i = 0; i < count; ++i) {
        T comp;
        ComponentSerializer<T>::deserialize(comp, s);
        dense_data.push(arena, comp);
      }
    }
    for (U64 i = 0; i < dense_entities.count; ++i) {
      Entity e      = dense_entities[i];
      USize  page   = e.index >> SHIFT;
      USize  offset = e.index & MASK;
      ensure_page(page);
      sparse_pages[page][offset] = static_cast<USize>(i);
    }
  }

  friend class RuntimeStateManager;
};

} // namespace sd

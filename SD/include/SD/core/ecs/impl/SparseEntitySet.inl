// TODO(invariants): Add assertions after mutations:
//   - Add(): assert(denseEntities.size() == denseData.size())
//   - Remove(): assert(denseEntities.size() == denseData.size()) after swap-and-pop
//   - Remove(): assert(sparse[lastPage][lastOffset] == denseIdx) after update
//   - DeserializeFrom(): call ValidateInvariants() at end
#pragma once

template<typename T>
template<typename... Args>
void SparseEntitySet<T>::add(Entity entity, Args&&... args) {
  const usize page   = entity.index >> SHIFT;
  const usize offset = entity.index & MASK;

  if (page >= sparse.size())
    sparse.resize(page + 1);
  if (!sparse[page]) {
    sparse[page] = std::make_unique<usize[]>(PAGE_SIZE);
    std::fill_n(sparse[page].get(), PAGE_SIZE, std::numeric_limits<usize>::max());
  }

  usize dense_idx = sparse[page][offset];
  if (dense_idx != std::numeric_limits<usize>::max()) {
    dense_data[dense_idx]     = T{std::forward<Args>(args)...};
    dense_entities[dense_idx] = entity;
  } else {
    sparse[page][offset] = dense_entities.size(); // end
    dense_data.push_back(T{std::forward<Args>(args)...});
    dense_entities.push_back(entity);
  }
#ifdef SD_DEBUG
  ValidateInvariants();
#endif
}
template<typename T>
bool SparseEntitySet<T>::remove(Entity entity) {
  const usize page   = entity.index >> SHIFT;
  const usize offset = entity.index & MASK;

  if (page >= sparse.size() || !sparse[page])
    return false; // out of bounds

  usize dense_idx = sparse[page][offset];
  if (dense_idx == std::numeric_limits<usize>::max())
    return false; // doesnt exist

  if (dense_entities[dense_idx] != entity)
    return false; // wrong generation

  // move, last to removed pos
  usize  last_idx    = dense_entities.size() - 1;
  Entity last_entity = dense_entities[last_idx];

  dense_data[dense_idx]     = std::move(dense_data[last_idx]);
  dense_entities[dense_idx] = last_entity;

  // update sparse index
  const usize last_page          = last_entity.index >> SHIFT;
  const usize last_offset        = last_entity.index & MASK;
  sparse[last_page][last_offset] = dense_idx;

  // free index
  sparse[page][offset] = std::numeric_limits<usize>::max();

  // remove duplicated last
  dense_data.pop_back();
  dense_entities.pop_back();
#ifdef SD_DEBUG
  ValidateInvariants();
#endif

  return true;
}
template<typename T>
T* SparseEntitySet<T>::get(Entity entity) {
  const usize page   = entity.index >> SHIFT;
  const usize offset = entity.index & MASK;

  if (page >= sparse.size() || !sparse[page])
    return nullptr;

  usize dense_idx = sparse[page][offset];

  if (dense_idx == std::numeric_limits<usize>::max())
    return nullptr; // cant get something that doesnt have data in this component.

  // Check if the entity is still alive
  if (dense_entities[dense_idx] != entity)
    return nullptr;


  return &dense_data[dense_idx];
}
template<typename T>
const T* SparseEntitySet<T>::get(const Entity entity) const {
  const usize page   = entity.index >> SHIFT;
  const usize offset = entity.index & MASK;

  if (page >= sparse.size() || !sparse[page])
    return nullptr;

  usize dense_idx = sparse[page][offset];

  if (dense_idx == std::numeric_limits<usize>::max())
    return nullptr;

  if (dense_entities[dense_idx] != entity)
    return nullptr;


  return &dense_data[dense_idx];
}
template<typename T>
std::optional<ComponentDebugInfo> SparseEntitySet<T>::get_debug_info(Entity e) {
  if constexpr (ComponentTraits<T>::s_is_registered) {
    T* ptr = get(e);
    if (!ptr)
      return std::nullopt;

    return ComponentDebugInfo{ComponentTraits<T>::id, ComponentTraits<T>::name, ptr};
  } else {
    return std::nullopt; // not a component
  }
}

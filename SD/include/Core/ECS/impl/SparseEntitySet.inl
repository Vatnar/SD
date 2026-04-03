// TODO(invariants): Add assertions after mutations:
//   - Add(): assert(denseEntities.size() == denseData.size())
//   - Remove(): assert(denseEntities.size() == denseData.size()) after swap-and-pop
//   - Remove(): assert(sparse[lastPage][lastOffset] == denseIdx) after update
//   - DeserializeFrom(): call ValidateInvariants() at end

template<typename T>
template<typename... Args>
void SparseEntitySet<T>::Add(Entity entity, Args&&... args) {
  const usize page = entity.index >> SHIFT;
  const usize offset = entity.index & MASK;

  if (page >= sparse.size())
    sparse.resize(page + 1);
  if (!sparse[page]) {
    sparse[page] = std::make_unique<usize[]>(PAGE_SIZE);
    std::fill_n(sparse[page].get(), PAGE_SIZE, std::numeric_limits<usize>::max());
  }

  usize denseIdx = sparse[page][offset];
  if (denseIdx != std::numeric_limits<usize>::max()) {
    denseData[denseIdx] = T{std::forward<Args>(args)...};
    denseEntities[denseIdx] = entity;
  } else {
    sparse[page][offset] = denseEntities.size(); // end
    denseData.push_back(T{std::forward<Args>(args)...});
    denseEntities.push_back(entity);
  }
}
template<typename T>
bool SparseEntitySet<T>::Remove(Entity entity) {
  const usize page = entity.index >> SHIFT;
  const usize offset = entity.index & MASK;

  if (page >= sparse.size() || !sparse[page])
    return false; // out of bounds

  usize denseIdx = sparse[page][offset];
  if (denseIdx == std::numeric_limits<usize>::max())
    return false; // doesnt exist

  if (denseEntities[denseIdx] != entity)
    return false; // wrong generation

  // move, last to removed pos
  usize lastIdx = denseEntities.size() - 1;
  Entity lastEntity = denseEntities[lastIdx];

  denseData[denseIdx] = std::move(denseData[lastIdx]);
  denseEntities[denseIdx] = lastEntity;

  // update sparse index
  const usize lastPage = lastEntity.index >> SHIFT;
  const usize lastOffset = lastEntity.index & MASK;
  sparse[lastPage][lastOffset] = denseIdx;

  // free index
  sparse[page][offset] = std::numeric_limits<usize>::max();

  // remove duplicated last
  denseData.pop_back();
  denseEntities.pop_back();


  return true;
}
template<typename T>
T* SparseEntitySet<T>::Get(Entity entity) {
  const usize page = entity.index >> SHIFT;
  const usize offset = entity.index & MASK;

  if (page >= sparse.size() || !sparse[page])
    return nullptr;

  usize denseIdx = sparse[page][offset];

  if (denseIdx == std::numeric_limits<usize>::max())
    return nullptr; // cant get something that doesnt have data in this component.

  // Check if the entity is still alive
  if (denseEntities[denseIdx] != entity)
    return nullptr;


  return &denseData[denseIdx];
}
template<typename T>
const T* SparseEntitySet<T>::Get(const Entity entity) const {
  const usize page = entity.index >> SHIFT;
  const usize offset = entity.index & MASK;

  if (page >= sparse.size() || !sparse[page])
    return nullptr;

  usize denseIdx = sparse[page][offset];

  if (denseIdx == std::numeric_limits<usize>::max())
    return nullptr;

  if (denseEntities[denseIdx] != entity)
    return nullptr;


  return &denseData[denseIdx];
}
template<typename T>
std::optional<ComponentDebugInfo> SparseEntitySet<T>::GetDebugInfo(Entity e) {
  if constexpr (ComponentTraits<T>::IsRegistered) {
    T* ptr = Get(e);
    if (!ptr)
      return std::nullopt;

    return ComponentDebugInfo{ComponentTraits<T>::Id, ComponentTraits<T>::Name, ptr};
  } else {
    return std::nullopt; // not a component
  }
}

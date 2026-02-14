template<typename T>
template<typename... Args>
void SparseEntitySet<T>::Add(Entity entity, Args&&... args) {
  const size_t page = entity.index >> SHIFT;
  const size_t offset = entity.index & MASK;

  if (page >= sparse.size())
    sparse.resize(page + 1);
  if (!sparse[page]) {
    sparse[page] = std::make_unique<size_t[]>(PAGE_SIZE);
    std::fill_n(sparse[page].get(), PAGE_SIZE, std::numeric_limits<size_t>::max());
  }

  size_t denseIdx = sparse[page][offset];
  if (denseIdx != std::numeric_limits<size_t>::max()) {
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
template<typename T>
T* SparseEntitySet<T>::Get(Entity entity) {
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
template<typename T>
const T* SparseEntitySet<T>::Get(const Entity entity) const {
  const size_t page = entity.index >> SHIFT;
  const size_t offset = entity.index & MASK;

  if (page >= sparse.size() || !sparse[page])
    return nullptr;

  size_t denseIdx = sparse[page][offset];

  if (denseIdx == std::numeric_limits<size_t>::max())
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

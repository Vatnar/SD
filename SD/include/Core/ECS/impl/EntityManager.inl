template<typename... Components>
ViewImpl<Components...>::ViewImpl(EntityManager& manager) : mManager(manager) {
  usize minSize = std::numeric_limits<usize>::max();

  (CheckSize<Components>(minSize), ...);
}
template<typename... Components>
ViewImpl<Components...>::Iterator::Iterator(EntityManager& em,
                                            const std::vector<Entity>* denseEntities, usize idx) :
  manager(em), entities(denseEntities), index(idx) {
  if (index < entities->size() && !IsValid()) {
    Next();
  }
}
template<typename... Components>
typename ViewImpl<Components...>::Iterator& ViewImpl<Components...>::Iterator::operator++() {
  Next();
  return *this;
}
template<typename... Components>
std::tuple<Entity, Components&...> ViewImpl<Components...>::Iterator::operator*() const {
  Entity currentEntity = (*entities)[index];
  return std::tuple_cat(std::make_tuple(currentEntity),
                        manager.GetComponentGroup<Components...>(currentEntity));
}
template<typename... Components>
void ViewImpl<Components...>::Iterator::Next() {
  do {
    index++;
  } while (index < entities->size() && !IsValid());
}
template<typename... Components>
bool ViewImpl<Components...>::Iterator::IsValid() const {
  Entity currentEntity = (*entities)[index];
  return (manager.HasComponent<Components>(currentEntity) && ...);
}
template<typename... Components>
ViewImpl<Components...>::Iterator ViewImpl<Components...>::begin() {
  if (!mSmallestPool) {
    SPDLOG_WARN("View has no valid component pools - scene may be empty or missing components");
    return end();
  }
  return Iterator(mManager, mSmallestPool, 0);
}
template<typename... Components>
ViewImpl<Components...>::Iterator ViewImpl<Components...>::end() {
  return Iterator(mManager, mSmallestPool, mSmallestPool ? mSmallestPool->size() : 0);
}
template<typename... Components>
template<typename Component>
void ViewImpl<Components...>::CheckSize(usize& minSize) {
  if (!mManager.HasComponentPool<Component>()) {
    minSize = 0;
    mSmallestPool = nullptr;
    return;
  }

  auto* pool = mManager.GetComponentPool<Component>();
  if (pool->Size() < minSize) {
    minSize = pool->Size();
    mSmallestPool = &pool->GetDenseEntities();
  }
}


template<typename T, typename... Args>
T* EntityManager::AddComponent(Entity e, Args&&... args) {
  static_assert(ComponentTraits<T>::IsRegistered,
                "Error: Component type is not registered, register it");
  const usize typeId = ComponentTraits<T>::Id;


  if (typeId >= mComponentPools.size())
    mComponentPools.resize(typeId + 1);
  if (!mComponentPools[typeId])
    mComponentPools[typeId] = std::make_unique<SparseEntitySet<T>>();

  // add data
  if (mEntityMasks[e]->test(typeId)) {
    spdlog::get("engine")->warn("Overwriting already existing component: {}, id: {} ",
                                ComponentTraits<T>::Name, typeId);
  }
  auto* pool = static_cast<SparseEntitySet<T>*>(mComponentPools[typeId].get());
  mEntityMasks[e]->set(typeId);
  pool->Add(e, std::forward<Args>(args)...);

  return pool->Get(e);
}
template<typename T>
T* EntityManager::TryGetComponent(Entity e) {
  static_assert(ComponentTraits<T>::IsRegistered,
                "Error: Component type is not registered, register it");

  usize typeId = ComponentTraits<T>::Id;
  if (typeId >= mComponentPools.size() || !mComponentPools[typeId] ||
      !mEntityMasks[e]->test(typeId))
    return nullptr;

  auto* pool = static_cast<SparseEntitySet<T>*>(mComponentPools[typeId].get());
  return pool->Get(e);
}
template<typename T>
T& EntityManager::GetComponent(Entity e) {
  usize typeId = ComponentTraits<T>::Id;
  assert(HasComponent<T>(e) && "Entity doesnt have component");

  auto* pool = static_cast<SparseEntitySet<T>*>(mComponentPools[typeId].get());
  return *pool->Get(e);
}

template<typename T>
bool EntityManager::TryRemoveComponent(Entity e) {
  static_assert(ComponentTraits<T>::IsRegistered,
                "Error: Can't remove component type that isn't registered");

  usize typeId = ComponentTraits<T>::Id;
  if (typeId >= mComponentPools.size() || !mComponentPools[typeId] ||
      !mEntityMasks[e]->test(typeId))
    return false;

  auto* pool = static_cast<SparseEntitySet<T>*>(mComponentPools[typeId].get());
  pool->Remove(e);
  mEntityMasks[e]->reset(typeId);
  return true;
}


template<typename... Args>
auto EntityManager::View() {
  if constexpr (sizeof...(Args) == 1) {
    using T = std::tuple_element_t<0, std::tuple<Args...>>;
    if constexpr (UnpackGroup<T>::IsGroup) {
      return typename UnpackGroup<T>::type(*this);
    } else {
      return ViewImpl<T>(*this);
    }
  } else {
    return ViewImpl<Args...>(*this);
  }
}
template<typename T>
bool EntityManager::HasComponent(Entity e) const {
  if (e.index >= mEntityMasks.GetDenseEntities().size())
    return false;

  auto mask = mEntityMasks.Get(e);
  return mask && mask->test(ComponentTraits<T>::Id);
}
template<typename... Components>
std::tuple<Components&...> EntityManager::GetComponentGroup(Entity e) {
  return std::forward_as_tuple(GetComponent<Components>(e)...);
}
template<typename... Components>
std::tuple<const Components&...> EntityManager::GetComponentGroup(Entity e) const {
  return std::forward_as_tuple(*GetComponent<Components>(e)...);
}
template<typename T>
SparseEntitySet<T>* EntityManager::GetComponentPool() {
  static_assert(ComponentTraits<T>::IsRegistered != false, "Unregistered Component");
  const usize typeId = ComponentTraits<T>::Id;
  if (typeId >= mComponentPools.size() || !mComponentPools[typeId])
    return nullptr;

  return static_cast<SparseEntitySet<T>*>(mComponentPools[typeId].get());
}
template<typename T>
bool EntityManager::HasComponentPool() {
  static_assert(ComponentTraits<T>::IsRegistered != false, "Unregistered Component");
  const usize typeId = ComponentTraits<T>::Id;
  return typeId < mComponentPools.size() && mComponentPools[typeId];
}
inline Entity EntityManager::Create() {
  const uint32_t idx = mFreeList.empty() ? mGenerations.size() : PopFreeList();

  if (idx >= mGenerations.size()) {
    mGenerations.resize(idx + 1, 0);
  }
  Entity e = {idx, mGenerations[idx]};

  mEntityMasks.Add(e, ComponentMask{});
  return e;
}
inline void EntityManager::Destroy(const Entity e) {
  if (!IsAlive(e))
    return;


  // TODO: Ranges?
  ComponentMask* mask = mEntityMasks[e];
  assert(mask);

  for (usize i = 0; i < mask->size(); ++i) {
    if (mask->test(i)) {
      if (mComponentPools[i])
        mComponentPools[i]->Remove(e);
    }
  }
  mask->reset();
  mGenerations[e.index]++;
  mFreeList.push_back(e.index);
}
inline std::vector<ComponentDebugInfo> EntityManager::GetAllComponentInfo(Entity e) const {
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
inline bool EntityManager::IsAlive(const Entity e) const {
  return e.index < mGenerations.size() && mGenerations[e.index] == e.generation;
}
inline uint32_t EntityManager::PopFreeList() {
  const auto idx = mFreeList.back();
  mFreeList.pop_back();
  return idx;
}

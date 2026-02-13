#pragma once
#include <cstdint>
#include <vector>
struct Entity {
  uint32_t index;
  uint32_t generation;

  bool operator==(Entity other) const;
};
inline bool Entity::operator==(Entity other) const {
  return index == other.index && generation == other.generation;
}


class EntityManager {
public:
  Entity Create() {
    const uint32_t idx = mFreeList.empty() ? mGenerations.size() : PopFreeList();

    if (idx >= mGenerations.size()) {
      mGenerations.resize(idx + 1, 0);
    }
    return Entity{idx, mGenerations[idx]};
  }

  void Destroy(const Entity e) {
    if (!IsAlive(e))
      return;

    mGenerations[e.index]++;
    mFreeList.push_back(e.index);

    // TODO: remove components
  }

  bool IsAlive(const Entity e) const { return mGenerations[e.index] == e.generation; }

private:
  uint32_t PopFreeList() {
    const auto idx = mFreeList.back();
    mFreeList.pop_back();
    return idx;
  }
  std::vector<uint32_t> mGenerations;
  std::vector<uint32_t> mFreeList;

  // TODO: Storage for each component system
};

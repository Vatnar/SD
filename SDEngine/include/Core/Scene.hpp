#pragma once
#include "ECS/EntityManager.hpp"

namespace SD {
class Scene {
public:
  EntityManager& GetEntityManager() { return mEntityManager; }
  const EntityManager& GetEntityManager() const { return mEntityManager; }

private:
  EntityManager mEntityManager; // TODO: i dont htink this supports multiple right now
};
} // namespace SD

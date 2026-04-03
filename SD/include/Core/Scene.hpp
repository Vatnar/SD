// TODO(docs): Add file-level Doxygen header
//   - @file Scene.hpp
//   - @brief Scene container - holds EntityManager and scene-level state
//   - Relationship to View and Application
//   - Scene lifecycle (OnStart, OnUpdate, OnStop)
#pragma once
#include "ECS/EntityManager.hpp"

namespace SD {
// TODO(docs): Document Scene class
//   - Purpose: Container for entities and scene-level systems
//   - Ownership of EntityManager
//   - Active/inactive state management
//   - Future: scene-level systems registration
//   - Example: Creating and populating a scene
class Scene {
public:
  explicit Scene(std::string name = "Untitled Scene") : mName(std::move(name)) {}

  void OnStart() { mActive = true; }
  void OnStop()  { mActive = false; }
  void OnUpdate(float dt) { (void)dt; /* future: scene-level systems */ }

  [[nodiscard]] bool IsActive() const { return mActive; }
  const std::string& GetName() const { return mName; }

  EntityManager em;

private:
  std::string mName;
  bool mActive = false;
};
} // namespace SD

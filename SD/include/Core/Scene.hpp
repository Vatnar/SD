#pragma once
#include "ECS/EntityManager.hpp"

namespace SD {
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

#include "RuntimeStateManager.hpp"

#include "Application.hpp"
#include "Core/Logging.hpp"

namespace SD {

RuntimeStateManager::RuntimeStateManager() = default;
RuntimeStateManager::~RuntimeStateManager() = default;

void RuntimeStateManager::Serialize() {
  if (!mApp) return;
  auto scenes = mApp->GetScenes();
  for (auto* scene : scenes) {
    // TODO: Implement ECS serialization when component serialization is ready
    SPDLOG_DEBUG("Serializing scene: {}", scene->GetName());
  }
}

void RuntimeStateManager::Restore(Application* app) {
  mApp = app;
  if (!app) return;
  auto scenes = app->GetScenes();
  for (auto* scene : scenes) {
    // TODO: Implement ECS deserialization when component serialization is ready
    SPDLOG_DEBUG("Restoring scene: {}", scene->GetName());
  }
}

void RuntimeStateManager::SetApplication(Application* app) {
  mApp = app;
}

bool RuntimeStateManager::HasState() const {
  return !mSerializedScenes.empty();
}

} // namespace SD

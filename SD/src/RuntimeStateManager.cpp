#include "RuntimeStateManager.hpp"

#include "Application.hpp"
#include "Core/Logging.hpp"

namespace SD {

RuntimeStateManager::RuntimeStateManager() = default;
RuntimeStateManager::~RuntimeStateManager() = default;

// TODO: this doesnt make sense since game doesnt own state
void RuntimeStateManager::Serialize() {
  if (!mApp)
    return;
  SD::Log::Engine::Debug("RuntimeStateManager::Serialize() called (state persists externally)");
}

void RuntimeStateManager::Restore(Application* app) {
  // Game state is externally owned and auto-persists across hot reloads.
  // Engine state does not need restoration here.
  mApp = app;
  if (!app)
    return;
  SD::Log::Engine::Debug("RuntimeStateManager::Restore() called (state persists externally)");
}

void RuntimeStateManager::SetApplication(Application* app) {
  mApp = app;
}

bool RuntimeStateManager::HasState() const {
  // mSerializedScenes is unused — state is externally owned.
  return false;
}

} // namespace SD

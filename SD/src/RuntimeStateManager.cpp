#include "SD/RuntimeStateManager.hpp"

#include "SD/Application.hpp"
#include "SD/core/logging.hpp"

namespace sd {

RuntimeStateManager::RuntimeStateManager()  = default;
RuntimeStateManager::~RuntimeStateManager() = default;

// TODO: this doesnt make sense since game doesnt own state
void RuntimeStateManager::serialize() {
  if (!m_app)
    return;
  log::engine::debug("RuntimeStateManager::Serialize() called (state persists externally)");
}

void RuntimeStateManager::restore(Application* app) {
  // Game state is externally owned and auto-persists across hot reloads.
  // Engine state does not need restoration here.
  m_app = app;
  if (!app)
    return;
  log::engine::debug("RuntimeStateManager::Restore() called (state persists externally)");
}

void RuntimeStateManager::set_application(Application* app) {
  m_app = app;
}

bool RuntimeStateManager::has_state() const {
  // mSerializedScenes is unused — state is externally owned.
  return false;
}

} // namespace sd

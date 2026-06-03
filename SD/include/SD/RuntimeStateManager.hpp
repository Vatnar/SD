/**
 * @file RuntimeStateManager.hpp
 * @brief State persistence for hot reload support
 *
 * @note Game state (scenes, entities, components) is externally owned and auto-persists
 *       across hot reloads. This class does NOT serialize/deserialize ECS state.
 *       It exists primarily to provide a hook point for future engine-level state transitions.
 */
#pragma once
#include <string>
#include <unordered_map>
#include <vector>


namespace sd {

class Application;
class Scene;

/**
 * Provides hooks for hot-reload state transitions.
 * Game state is externally owned and auto-persists across reloads.
 */
class RuntimeStateManager {
public:
  RuntimeStateManager();
  ~RuntimeStateManager();

  void serialize();
  void restore(Application* app);
  void set_application(Application* app);
  bool has_state() const;

private:
  Application*                                       m_app = nullptr;
  std::unordered_map<std::string, std::vector<char>> m_serialized_scenes;
};

} // namespace sd

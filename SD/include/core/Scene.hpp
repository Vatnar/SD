// TODO(docs): Add file-level Doxygen header
//   - @file Scene.hpp
//   - @brief Scene container - holds EntityManager and scene-level state
//   - Relationship to View and Application
//   - Scene lifecycle (OnStart, OnUpdate, OnStop)
#pragma once
#include "ecs/CommandQueue.hpp"
#include "ecs/EntityManager.hpp"

namespace sd {
// TODO(docs): Document Scene class
//   - Purpose: Container for entities and scene-level systems
//   - Ownership of EntityManager
//   - Active/inactive state management
//   - Future: scene-level systems registration
//   - Example: Creating and populating a scene
class Scene {
public:
  explicit Scene(std::string name = "Untitled Scene") : m_name(std::move(name)) {}

  void on_start() { m_is_active = true; }
  void on_stop() { m_is_active = false; }
  void on_update(float dt) { (void)dt; /* future: scene-level systems */ }

  [[nodiscard]] bool is_active() const { return m_is_active; }
  [[nodiscard]] const std::string& get_name() const { return m_name; }

  template<typename T, typename... Args>
  void add_command(Args&&... args) {
    m_commands.add<T>(std::forward<Args>(args)...);
  }

  void apply_commands() {
    m_commands.apply(em);
    m_commands.clear();
  }
  [[nodiscard]] usize command_count() const;

  EntityManager em;

private:
  CommandQueue m_commands;
  std::string m_name;
  bool m_is_active = false;
};
inline usize Scene::command_count() const {
  return m_commands.get_count();
}
} // namespace SD

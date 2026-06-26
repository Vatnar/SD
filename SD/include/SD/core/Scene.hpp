#pragma once

#include "SD/arena.hpp"
#include "ecs/CommandQueue.hpp"
#include "ecs/EntityManager.hpp"

namespace sd {

struct Scene {
  explicit Scene(std::string name = "Untitled Scene", Arena* pool_arena = nullptr) :
    m_name(std::move(name)) {
    em.m_pool_arena = pool_arena;
  }

  Scene(const Scene&)            = delete;
  Scene& operator=(const Scene&) = delete;

  void on_start() { m_is_active = true; }
  void on_stop() { m_is_active = false; }
  void on_update(float dt) { (void)dt; }

  [[nodiscard]] bool               is_active() const { return m_is_active; }
  [[nodiscard]] const std::string& get_name() const { return m_name; }

  template<typename T, typename... Args>
  void add_command(Args&&... args) {
    m_commands.add<T>(std::forward<Args>(args)...);
  }

  void apply_commands() {
    m_commands.apply(em);
    m_commands.clear();
  }

  [[nodiscard]] USize command_count() const;

  EntityManager<ComponentGroup<>> em;

  CommandQueue m_commands;
  std::string  m_name;
  bool         m_is_active = false;
};

inline USize Scene::command_count() const {
  return m_commands.get_count();
}

} // namespace sd

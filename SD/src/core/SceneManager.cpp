#include "SD/core/SceneManager.hpp"

#include "SD/core/Scene.hpp"
#include "SD/core/logging.hpp"

namespace sd {

Scene* SceneManager::create(Arena* arena, const std::string& name) {
  for (auto* scene : m_scenes) {
    if (scene->get_name() == name) {
      log::engine::warn("Scene '{}' already exists, returning existing scene", name);
      return scene;
    }
  }
  auto* scene = arena_push<Scene>(arena);
  new (scene) Scene(name, arena);
  m_scenes.push_back(scene);
  return m_scenes.back();
}

Scene* SceneManager::get(const std::string& name) const {
  for (auto* scene : m_scenes) {
    if (scene->get_name() == name) {
      return scene;
    }
  }
  return nullptr;
}

void SceneManager::clear() {
  m_scenes.clear();
}

} // namespace sd

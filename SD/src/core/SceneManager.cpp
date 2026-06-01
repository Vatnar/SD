#include "core/SceneManager.hpp"

#include "core/Scene.hpp"
#include "core/logging.hpp"

namespace sd {

Scene* SceneManager::create(const std::string& name) {
  for (auto& scene : m_scenes) {
    if (scene->get_name() == name) {
      log::engine::warn("Scene '{}' already exists, returning existing scene", name);
      return scene.get();
    }
  }
  auto scene = std::make_unique<Scene>(name);
  m_scenes.push_back(std::move(scene));
  return m_scenes.back().get();
}

Scene* SceneManager::get(const std::string& name) const {
  for (auto& scene : m_scenes) {
    if (scene->get_name() == name) {
      return scene.get();
    }
  }
  return nullptr;
}

void SceneManager::clear() {
  m_scenes.clear();
}

} // namespace sd
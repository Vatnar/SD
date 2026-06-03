#pragma once

#include <memory>
#include <string>
#include <vector>

#include "SD/export.hpp"


namespace sd {

class Scene;

class SD_EXPORT SceneManager {
public:
  SceneManager() = default;

  Scene* create(const std::string& name);
  Scene* get(const std::string& name) const;

  template<typename F>
  void for_each(F&& fn) {
    for (auto& scene : m_scenes) {
      fn(*scene);
    }
  }

  template<typename F>
  void for_each(F&& fn) const {
    for (const auto& scene : m_scenes) {
      fn(*scene);
    }
  }

  void clear();

private:
  std::vector<std::unique_ptr<Scene>> m_scenes;
};

} // namespace sd

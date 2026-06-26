#pragma once

#include <string>
#include <vector>

#include "SD/arena.hpp"
#include "SD/export.hpp"


namespace sd {

struct Scene;

struct SD_EXPORT SceneManager {
  Scene* create(Arena* arena, const std::string& name);
  Scene* get(const std::string& name) const;

  template<typename F>
  void for_each(F&& fn) {
    for (auto* scene : m_scenes) {
      fn(*scene);
    }
  }

  template<typename F>
  void for_each(F&& fn) const {
    for (const auto* scene : m_scenes) {
      fn(*scene);
    }
  }

  void clear();

  std::vector<Scene*> m_scenes;
};

} // namespace sd

// TODO(docs): Add file-level Doxygen header
//   - @file ViewManager.hpp
//   - @brief Manages View instances (create, destroy, query)
//   - Relationship to Application and View classes
#pragma once

#include <expected>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "SD/core/Scene.hpp"
#include "SD/core/View.hpp"
#include "SD/core/base.hpp"
#include "SD/export.hpp"

namespace sd {

// TODO(docs): Document ViewManager class
//   - Purpose: Factory and registry for View objects
//   - View naming and ID system
//   - Error handling patterns (std::expected)
//   - Example: Creating and managing multiple views
class SD_EXPORT ViewManager {
public:
  ViewManager();
  ~ViewManager();

  template<typename T, typename... Args>
    requires std::is_base_of_v<View, T>
  T& create(std::string name, Args&&... args) {
    if (m_view_name_to_id.contains(name))
      NOT_IMPLEMENTED;

    ViewId id       = m_next_view_id++;
    auto   view     = std::make_unique<T>(std::move(name), std::forward<Args>(args)...);
    view->m_view_id = id;

    auto& ref = *view;
    m_views_by_id.emplace(id, std::move(view));
    m_view_name_to_id.emplace(ref.get_name(), id);
    return ref;
  }

  using ViewResult = std::expected<std::reference_wrapper<View>, ViewError>;

  ViewResult                       get(ViewId id);
  ViewResult                       get(const std::string& name);
  std::expected<ViewId, ViewError> get_id(const std::string& name) const;

  ViewError remove(ViewId id);
  ViewError remove(const std::string& name);

  template<typename T, typename... Args>
    requires std::is_base_of_v<Layer, T>
  std::expected<std::reference_wrapper<T>, ViewError> push_layer(ViewId id, Args&&... args) {
    auto it = m_views_by_id.find(id);
    if (it == m_views_by_id.end())
      return std::unexpected(VIEW_DOES_NOT_EXIST);
    return it->second->push_layer<T>(std::forward<Args>(args)...);
  }

  const std::unordered_map<ViewId, std::unique_ptr<View>>& get_views() const {
    return m_views_by_id;
  }
  auto& get_views() { return m_views_by_id; }

  template<typename F>
  void for_each(F&& fn) {
    for (auto& [id, view] : m_views_by_id) {
      fn(*view);
    }
  }

  template<typename F>
  void for_each(F&& fn) const {
    for (const auto& [id, view] : m_views_by_id) {
      fn(*view);
    }
  }

  std::vector<Scene*> get_scenes();

  void update_views(float dt);
  void render_views(vk::CommandBuffer cmd);
  void cleanup_closed_views();
  void clear();

private:
  std::unordered_map<ViewId, std::unique_ptr<View>> m_views_by_id;
  std::unordered_map<std::string, ViewId>           m_view_name_to_id;
  ViewId                                            m_next_view_id;
};

} // namespace sd

#include "core/ViewManager.hpp"

#include <algorithm>

namespace sd {

ViewManager::ViewManager()  = default;
ViewManager::~ViewManager() = default;

ViewManager::ViewResult ViewManager::get(ViewId id) {
  auto it = m_views_by_id.find(id);
  if (it == m_views_by_id.end())
    return std::unexpected(VIEW_DOES_NOT_EXIST);
  ASSERT(it->second && "View must be valid");
  return std::ref(*it->second);
}

ViewManager::ViewResult ViewManager::get(const std::string& name) {
  ASSERT(!name.empty() && "View name must not be empty");
  auto it_name = m_view_name_to_id.find(name);
  if (it_name == m_view_name_to_id.end())
    return std::unexpected(VIEW_DOES_NOT_EXIST);
  return get(it_name->second);
}

std::expected<ViewId, ViewError> ViewManager::GetId(const std::string& name) const {
  ASSERT(!name.empty() && "View name must not be empty");
  auto it_name = m_view_name_to_id.find(name);
  if (it_name == m_view_name_to_id.end())
    return std::unexpected(VIEW_DOES_NOT_EXIST);
  return it_name->second;
}

ViewError ViewManager::remove(ViewId id) {
  auto it = m_views_by_id.find(id);
  if (it == m_views_by_id.end())
    return VIEW_DOES_NOT_EXIST;

  m_view_name_to_id.erase(it->second->get_name());
  m_views_by_id.erase(it);
  return SUCCESS;
}

ViewError ViewManager::remove(const std::string& name) {
  ASSERT(!name.empty() && "View name must not be empty");
  auto it_name = m_view_name_to_id.find(name);
  if (it_name == m_view_name_to_id.end())
    return VIEW_DOES_NOT_EXIST;

  // Avoid second lookup by erasing directly
  ViewId id      = it_name->second;
  auto   it_view = m_views_by_id.find(id);
  if (it_view == m_views_by_id.end())
    return VIEW_DOES_NOT_EXIST; // Shouldn't happen, but be safe

  m_view_name_to_id.erase(it_name);
  m_views_by_id.erase(it_view);
  return SUCCESS;
}

std::vector<Scene*> ViewManager::get_scenes() {
  std::vector<Scene*> scenes;
  scenes.reserve(m_views_by_id.size() * 2); // Prevent reallocation/iterator invalidation
  for (auto& view : m_views_by_id | std::views::values) {
    ASSERT(view && "View must be valid");
    for (auto& layer : view->get_layers()) {
      Scene* s = layer->get_scene();
      if (s && std::ranges::find(scenes, s) == scenes.end())
        scenes.push_back(s);
    }
  }
  return scenes;
}

void ViewManager::update_views(float dt) {
  for (auto& view : m_views_by_id | std::views::values) {
    ASSERT(view && "View must be valid");
    if (view->is_open()) {
      view->on_update(dt);
      view->on_gui_render();
    }
  }
}

void ViewManager::render_views(vk::CommandBuffer cmd) {
  ASSERT(cmd && "Command buffer must be valid");
  for (auto& view : m_views_by_id | std::views::values) {
    ASSERT(view && "View must be valid");
    if (view->is_open()) {
      view->on_render(cmd);
    }
  }
}

void ViewManager::cleanup_closed_views() {
  for (auto it = m_views_by_id.begin(); it != m_views_by_id.end();) {
    ASSERT(it->second && "View must be valid");
    if (!it->second->is_open()) {
      m_view_name_to_id.erase(it->second->get_name());
      it = m_views_by_id.erase(it);
    } else {
      ++it;
    }
  }
}

void ViewManager::clear() {
  m_views_by_id.clear();
  m_view_name_to_id.clear();
  m_next_view_id = ViewId{};
}

} // namespace sd

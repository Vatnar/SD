#include "Core/ViewManager.hpp"
#include <algorithm>

namespace SD {

ViewManager::ViewManager() = default;
ViewManager::~ViewManager() = default;

ViewManager::ViewResult ViewManager::Get(ViewId id) {
  auto it = mViewsById.find(id);
  if (it == mViewsById.end())
    return std::unexpected(ViewDoesNotExist);
  return std::ref(*it->second);
}

ViewManager::ViewResult ViewManager::Get(const std::string& name) {
  auto itName = mViewNameToId.find(name);
  if (itName == mViewNameToId.end())
    return std::unexpected(ViewDoesNotExist);
  return Get(itName->second);
}

std::expected<ViewId, ViewError> ViewManager::GetId(const std::string& name) const {
  auto itName = mViewNameToId.find(name);
  if (itName == mViewNameToId.end())
    return std::unexpected(ViewDoesNotExist);
  return itName->second;
}

ViewError ViewManager::Remove(ViewId id) {
  auto it = mViewsById.find(id);
  if (it == mViewsById.end())
    return ViewDoesNotExist;

  mViewNameToId.erase(it->second->GetName());
  mViewsById.erase(it);
  return Success;
}

ViewError ViewManager::Remove(const std::string& name) {
  auto itName = mViewNameToId.find(name);
  if (itName == mViewNameToId.end())
    return ViewDoesNotExist;
  return Remove(itName->second);
}

std::vector<Scene*> ViewManager::GetScenes() {
  std::vector<Scene*> scenes;
  for (auto& [id, view] : mViewsById) {
    for (auto& layer : view->GetLayers()) {
      Scene* s = layer->GetScene();
      if (s && std::find(scenes.begin(), scenes.end(), s) == scenes.end())
        scenes.push_back(s);
    }
  }
  return scenes;
}

void ViewManager::UpdateViews(float dt) {
  for (auto& [id, view] : mViewsById) {
    if (view->IsOpen()) {
      view->OnUpdate(dt);
      view->OnGuiRender();
    }
  }
}

void ViewManager::RenderViews(vk::CommandBuffer cmd) {
  for (auto& [id, view] : mViewsById) {
    if (view->IsOpen()) {
      view->OnRender(cmd);
    }
  }
}

void ViewManager::CleanupClosedViews() {
  for (auto it = mViewsById.begin(); it != mViewsById.end();) {
    if (!it->second->IsOpen()) {
      mViewNameToId.erase(it->second->GetName());
      it = mViewsById.erase(it);
    } else {
      ++it;
    }
  }
}

} // namespace SD

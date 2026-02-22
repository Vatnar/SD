#include "Core/ViewManager.hpp"
#include <algorithm>

#include "Utils/Utils.hpp"

namespace SD {

ViewManager::ViewManager() = default;
ViewManager::~ViewManager() = default;

ViewManager::ViewResult ViewManager::Get(ViewId id) {
  auto it = mViewsById.find(id);
  if (it == mViewsById.end())
    return std::unexpected(ViewDoesNotExist);
  SD_ASSERT(it->second, "View must be valid");
  return std::ref(*it->second);
}

ViewManager::ViewResult ViewManager::Get(const std::string& name) {
  SD_ASSERT(!name.empty(), "View name must not be empty");
  auto itName = mViewNameToId.find(name);
  if (itName == mViewNameToId.end())
    return std::unexpected(ViewDoesNotExist);
  return Get(itName->second);
}

std::expected<ViewId, ViewError> ViewManager::GetId(const std::string& name) const {
  SD_ASSERT(!name.empty(), "View name must not be empty");
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
  SD_ASSERT(!name.empty(), "View name must not be empty");
  auto itName = mViewNameToId.find(name);
  if (itName == mViewNameToId.end())
    return ViewDoesNotExist;
  return Remove(itName->second);
}

std::vector<Scene*> ViewManager::GetScenes() {
  std::vector<Scene*> scenes;
  for (auto& [id, view] : mViewsById) {
    SD_ASSERT(view, "View must be valid");
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
    SD_ASSERT(view, "View must be valid");
    if (view->IsOpen()) {
      view->OnUpdate(dt);
      view->OnGuiRender();
    }
  }
}

void ViewManager::RenderViews(vk::CommandBuffer cmd) {
  SD_ASSERT(cmd, "Command buffer must be valid");
  for (auto& [id, view] : mViewsById) {
    SD_ASSERT(view, "View must be valid");
    if (view->IsOpen()) {
      view->OnRender(cmd);
    }
  }
}

void ViewManager::CleanupClosedViews() {
  for (auto it = mViewsById.begin(); it != mViewsById.end();) {
    SD_ASSERT(it->second, "View must be valid");
    if (!it->second->IsOpen()) {
      mViewNameToId.erase(it->second->GetName());
      it = mViewsById.erase(it);
    } else {
      ++it;
    }
  }
}

void ViewManager::Clear() {
  mViewsById.clear();
  mViewNameToId.clear();
  mNextViewId = 0;
}

} // namespace SD

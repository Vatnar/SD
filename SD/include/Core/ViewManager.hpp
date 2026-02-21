#pragma once

#include "Core/Base.hpp"
#include "Core/View.hpp"
#include "Core/Scene.hpp"

#include <expected>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace SD {

using ViewId = u32;

class ViewManager {
public:
  ViewManager();
  ~ViewManager();

  template<typename T, typename... Args>
    requires std::is_base_of_v<View, T>
  T& Create(std::string name, Args&&... args) {
    if (mViewNameToId.contains(name))
      Abort("View name already exists: " + name);

    ViewId id = mNextViewId++;
    auto view = std::make_unique<T>(std::move(name), std::forward<Args>(args)...);
    view->mViewId = id;

    auto& ref = *view;
    mViewsById.emplace(id, std::move(view));
    mViewNameToId.emplace(ref.GetName(), id);
    return ref;
  }

  using ViewResult = std::expected<std::reference_wrapper<View>, ViewError>;

  ViewResult Get(ViewId id);
  ViewResult Get(const std::string& name);
  std::expected<ViewId, ViewError> GetId(const std::string& name) const;

  ViewError Remove(ViewId id);
  ViewError Remove(const std::string& name);

  template<typename T, typename... Args>
    requires std::is_base_of_v<Layer, T>
  std::expected<std::reference_wrapper<T>, ViewError> PushLayer(ViewId id, Args&&... args) {
    auto it = mViewsById.find(id);
    if (it == mViewsById.end())
      return std::unexpected(ViewDoesNotExist);
    return it->second->PushLayer<T>(std::forward<Args>(args)...);
  }

  const std::unordered_map<ViewId, std::unique_ptr<View>>& GetViews() const { return mViewsById; }
  auto& GetViews() { return mViewsById; }

  std::vector<Scene*> GetScenes();

  void UpdateViews(float dt);
  void RenderViews(vk::CommandBuffer cmd);
  void CleanupClosedViews();

private:
  std::unordered_map<ViewId, std::unique_ptr<View>> mViewsById;
  std::unordered_map<std::string, ViewId> mViewNameToId;
  ViewId mNextViewId = 0;
};

} // namespace SD

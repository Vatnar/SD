#pragma once
#include <expected>
#include <unordered_map>

#include "Core/FrameTimer.hpp"
#include "Core/GlfwContext.hpp"
#include "Core/LayerList.hpp"
#include "Core/SDImGuiContext.hpp"
#include "Core/View.hpp"
#include "Core/ViewManager.hpp"
#include "Core/Vulkan/VulkanContext.hpp"
#include "Core/Vulkan/VulkanRenderer.hpp"
#include "Core/WindowManager.hpp"
#include "RuntimeStateManager.hpp"

namespace SD {

struct ApplicationSpecification {
  std::string name = "SDEngine App";
  int width = 1600;
  int height = 900;
  bool enableHotReload = true;
};

class Application {
public:
  Application(const ApplicationSpecification& spec, RuntimeStateManager* stateManager = nullptr);
  virtual ~Application();

  void Run(std::atomic<bool>* externalStop);
  void Close() { mRunning = false; }
  void OnAppEvent(Event& e);

  // Layer management
  template<typename T, typename... Args>
    requires std::is_base_of_v<Layer, T>
  T& PushGlobalLayer(Args&&... args) {
    return mGlobalLayers.PushLayer<T>(std::forward<Args>(args)...);
  }

  // Window management
  WindowId CreateWindow(const WindowProps& props) { return mWindowManager->Create(props); }
  Window& GetWindow(WindowId id) { return mWindowManager->GetWindow(id); }
  VulkanWindow& GetRenderWindow(WindowId id) { return mWindowManager->GetRenderWindow(id); }

  template<typename T, typename... Args>
    requires std::is_base_of_v<Layer, T>
  T& PushViewLayer(WindowId id, Args&&... args) {
    auto& windows = mWindowManager->GetWindows();
    if (windows.find(id) == windows.end()) {
      Abort(std::format("Attempted to push layer to invalid window ID: {}", id));
    }
    return windows[id].viewLayers.PushLayer<T>(std::forward<Args>(args)...);
  }

  // View management
  template<typename T, typename... Args>
    requires std::is_base_of_v<View, T>
  T& CreateView(std::string name, Args&&... args) {
    return mViewManager->Create<T>(std::move(name), std::forward<Args>(args)...);
  }

  using ViewResult = ViewManager::ViewResult;

  ViewResult GetView(ViewId id) { return mViewManager->Get(id); }
  ViewResult GetView(const std::string& name) { return mViewManager->Get(name); }
  std::expected<ViewId, ViewError> GetViewId(const std::string& name) const {
    return mViewManager->GetId(name);
  }

  ViewError RemoveView(ViewId id) { return mViewManager->Remove(id); }
  ViewError RemoveView(const std::string& name) { return mViewManager->Remove(name); }

  template<typename T, typename... Args>
    requires std::is_base_of_v<Layer, T>
  std::expected<std::reference_wrapper<T>, ViewError> PushLayerToView(ViewId id, Args&&... args) {
    return mViewManager->PushLayer<T>(id, std::forward<Args>(args)...);
  }

  template<typename T, typename... Args>
    requires std::is_base_of_v<Layer, T>
  std::expected<std::reference_wrapper<T>, ViewError> PushLayerToView(const std::string& name,
                                                                      Args&&... args) {
    auto idRes = mViewManager->GetId(name);
    if (!idRes)
      return std::unexpected(idRes.error());
    return mViewManager->PushLayer<T>(*idRes, std::forward<Args>(args)...);
  }

  const std::unordered_map<ViewId, std::unique_ptr<View>>& GetViews() const {
    return mViewManager->GetViews();
  }
  std::vector<Scene*> GetScenes();

  // Hot Reload
  void SetHotReloadEnabled(bool enabled) { mHotReloadEnabled = enabled; }
  bool IsHotReloadEnabled() const { return mHotReloadEnabled; }
  void ReloadShaders();

  // Accessors
  VulkanContext& GetVulkanContext() { return *mVulkanCtx; }
  VulkanRenderer& GetRenderer() { return *mRenderer; }
  SDImGuiContext& GetImGuiContext() { return *mImGuiCtx; }
  WindowManager& GetWindowManager() { return *mWindowManager; }
  ViewManager& GetViewManager() { return *mViewManager; }

  static Application& Get() { return *sInstance; }
  float GetFrameWorkTime() const { return mTimer.GetFrameWorkTime(); }
  void AddGpuWaitTime(float t) { mTimer.AddGpuWaitTime(t); }
  FrameTimer& GetFrameTimer() { return mTimer; }

private:
  static Application* sInstance;
  ApplicationSpecification mSpec;
  bool mRunning = true;
  bool mHotReloadEnabled = true;
  float mHotReloadTimer = 0.0f;

  std::unique_ptr<GlfwContext> mGlfwCtx;
  std::unique_ptr<VulkanContext> mVulkanCtx;
  std::unique_ptr<VulkanRenderer> mRenderer;
  std::unique_ptr<SDImGuiContext> mImGuiCtx;

  std::unique_ptr<WindowManager> mWindowManager;
  std::unique_ptr<ViewManager> mViewManager;

  LayerList mGlobalLayers;
  EventManager mAppEventManager;

  FrameTimer mTimer;
  RuntimeStateManager* mStateManager;
};

} // namespace SD

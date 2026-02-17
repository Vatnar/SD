#pragma once
#include <expected>
#include <unordered_map>

#include "Core/GlfwContext.hpp"
#include "Core/LayerList.hpp"
#include "Core/SDImGuiContext.hpp"
#include "Core/View.hpp"
#include "Core/Vulkan/VulkanContext.hpp"
#include "Core/Vulkan/VulkanRenderer.hpp"

namespace SD {

using WindowId = u32;

struct ApplicationSpecification {
  std::string name = "SDEngine App";
  int width = 1600;
  int height = 900;
};

struct WindowProps {
  std::string title;
  int width, height;
  explicit WindowProps(const std::string& title = "SD Engine", int width = 1280, int height = 720) :
    title(title), width(width), height(height) {}
};

class Application {
public:
  Application(const ApplicationSpecification& spec);
  virtual ~Application();

  void Run();
  void Close() { mRunning = false; }
  void OnAppEvent(Event& e);

  // Layer management
  template<typename T, typename... Args>
    requires std::is_base_of_v<Layer, T>
  T& PushGlobalLayer(Args&&... args) {
    return mGlobalLayers.PushLayer<T>(std::forward<Args>(args)...);
  }

  // Window management
  WindowId CreateWindow(const WindowProps& props);
  Window& GetWindow(WindowId id);
  VulkanWindow& GetRenderWindow(WindowId id);

  template<typename T, typename... Args>
    requires std::is_base_of_v<Layer, T>
  T& PushViewLayer(WindowId id, Args&&... args) {
    if (mWindows.find(id) == mWindows.end()) {
      Abort(std::format("Attempted to push layer to invalid window ID: {}", id));
    }
    return mWindows[id].viewLayers.PushLayer<T>(std::forward<Args>(args)...);
  }

  // View management
  template<typename T, typename... Args>
    requires std::is_base_of_v<View, T>
  std::expected<ViewId, ViewError> CreateView(std::string name, Args&&... args) {
    if (mViewNameToId.contains(name))
      return std::unexpected(NameAlreadyExists);

    ViewId id = mNextViewId++;
    auto view = std::make_unique<T>(std::move(name), std::forward<Args>(args)...);

    auto& ref = *view;
    mViewsById.emplace(id, std::move(view));
    mViewNameToId.emplace(ref.GetName(), id);
    return id;
  }
  using ViewResult = std::expected<std::reference_wrapper<View>, ViewError>;

  ViewResult GetView(ViewId id) {
    auto it = mViewsById.find(id);
    if (it == mViewsById.end())
      return std::unexpected(ViewDoesNotExist);
    return std::ref(*it->second);
  }
  ViewResult GetView(const std::string& name) {
    auto itName = mViewNameToId.find(name);
    if (itName == mViewNameToId.end())
      return std::unexpected(ViewDoesNotExist);
    return GetView(itName->second);
  }
  std::expected<ViewId, ViewError> GetViewId(const std::string& name) const {
    auto itName = mViewNameToId.find(name);
    if (itName == mViewNameToId.end())
      return std::unexpected(ViewDoesNotExist);
    return itName->second;
  }

  ViewError RemoveView(ViewId id) {
    auto it = mViewsById.find(id);
    if (it == mViewsById.end())
      return ViewDoesNotExist;

    mViewNameToId.erase(it->second->GetName());
    mViewsById.erase(it);
    return Success;
  }

  ViewError RemoveView(const std::string& name) {
    auto itName = mViewNameToId.find(name);
    if (itName == mViewNameToId.end())
      return ViewDoesNotExist;
    return RemoveView(itName->second);
  }
  template<typename T, typename... Args>
    requires std::is_base_of_v<Layer, T>
  std::expected<std::reference_wrapper<T>, ViewError> PushLayerToView(ViewId id, Args&&... args) {
    if (!mViewsById.contains(id))
      return std::unexpected(ViewDoesNotExist);
    return mViewsById.at(id)->PushLayer<T>(std::forward<Args>(args)...);
  }

  template<typename T, typename... Args>
    requires std::is_base_of_v<Layer, T>
  std::expected<std::reference_wrapper<T>, ViewError> PushLayerToView(const std::string& name,
                                                                      Args&&... args) {
    if (!mViewNameToId.contains(name))
      return std::unexpected(ViewDoesNotExist);
    if (!mViewsById.contains(mViewNameToId[name]))
      return std::unexpected(ViewDoesNotExist);

    return mViewsById.at(mViewNameToId.at(name))->PushLayer<T>(std::forward<Args>(args)...);
  }


  // Accessors
  VulkanContext& GetVulkanContext() { return *mVulkanCtx; }
  SDImGuiContext& GetImGuiContext() { return *mImGuiCtx; }
  static Application& Get() { return *sInstance; }
  void DestroyWindow(WindowId id);

private:
  struct WindowData {
    std::unique_ptr<Window> logic;
    std::unique_ptr<VulkanWindow> render;
    LayerList viewLayers;

    WindowData() = default;
    WindowData(WindowData&&) = default;
    WindowData& operator=(WindowData&&) = default;
    WindowData(const WindowData&) = delete;
    WindowData& operator=(const WindowData&) = delete;
  };

  void UpdateWindow(WindowId id, WindowData& data, float dt);
  void DrawWindow(WindowId id, WindowData& data);

  static Application* sInstance;
  ApplicationSpecification mSpec;
  bool mRunning = true;

  std::unique_ptr<GlfwContext> mGlfwCtx;
  std::unique_ptr<VulkanContext> mVulkanCtx;
  std::unique_ptr<VulkanRenderer> mRenderer;
  std::unique_ptr<SDImGuiContext> mImGuiCtx;


  LayerList mGlobalLayers;
  EventManager mAppEventManager;


  std::unordered_map<WindowId, WindowData> mWindows;
  WindowId mNextWindowId = 0;

  std::unordered_map<ViewId, std::unique_ptr<View>> mViewsById;
  std::unordered_map<std::string, ViewId> mViewNameToId;
  ViewId mNextViewId = 0;

  std::vector<WindowId> mPendingClose;

  double mLastTime = 0.0;
  double mAccumulator = 0.0;
  const double mFixedTimeStep = 1.0 / 60.0;
};

} // namespace SD

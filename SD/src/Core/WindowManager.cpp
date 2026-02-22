#include "Core/WindowManager.hpp"

#include "Application.hpp"
#include "Core/Events/window/WindowEvents.hpp"
#include "Core/Logging.hpp"
#include "Core/ViewManager.hpp"

#include "Utils/Utils.hpp"

namespace SD {

WindowManager::WindowManager() = default;
WindowManager::~WindowManager() = default;

WindowId WindowManager::Create(const WindowProps& props) {
  SD_ASSERT(!props.title.empty(), "Window title must not be empty");
  SD_ASSERT(props.width > 0 && props.height > 0, "Window dimensions must be positive");

  WindowId id = mNextWindowId++;

  auto window =
      WindowBuilder().SetTitle(props.title.c_str()).SetSize(props.width, props.height).Build();

  SD_ASSERT(window, "Window must be created");

  auto& app = Application::Get();
  auto& vulkanCtx = app.GetVulkanContext();

  if (!vulkanCtx.IsInitialized()) {
    vulkanCtx.Init(*window);
  }

  auto vw = std::make_unique<VulkanWindow>(*window, vulkanCtx);

  WindowData data;
  data.logic = std::move(window);
  data.render = std::move(vw);

  mWindows[id] = std::move(data);
  return id;
}

void WindowManager::Destroy(WindowId id) {
  auto it = mWindows.find(id);
  SD_ASSERT(it != mWindows.end(), "Cannot destroy window - window ID does not exist");

  auto& app = Application::Get();
  (void)app.GetVulkanContext().GetVulkanDevice()->waitIdle();
  mWindows.erase(id);
}

void WindowManager::ProcessPendingCloses() {
  for (WindowId id : mPendingClose) {
    Destroy(id);
  }
  mPendingClose.clear();
}

Window& WindowManager::GetWindow(WindowId id) {
  auto it = mWindows.find(id);
  SD_ASSERT(it != mWindows.end(), "Window ID does not exist");
  return *it->second.logic;
}

VulkanWindow& WindowManager::GetRenderWindow(WindowId id) {
  auto it = mWindows.find(id);
  SD_ASSERT(it != mWindows.end(), "Window ID does not exist");
  SD_ASSERT(it->second.render, "Render window must be valid");
  return *it->second.render;
}

void WindowManager::UpdateWindows(float dt) {
  for (auto& [id, data] : mWindows) {
    UpdateWindow(id, data, dt);
  }
}

void WindowManager::DrawWindows(ViewManager& viewManager) {
  SD_ASSERT(!mWindows.empty(), "Must have at least one window to draw");

  for (auto& [id, data] : mWindows) {
    DrawWindow(id, data, viewManager);
  }
}

void WindowManager::UpdateWindow(WindowId id, WindowData& data, float dt) {
  SD_ASSERT(data.logic, "Logic window must be valid");
  SD_ASSERT(data.render, "Render window must be valid");

  Window& window = *data.logic;
  VulkanWindow& vw = *data.render;
  LayerList& layers = data.viewLayers;
  auto& app = Application::Get();

  if (vw.IsMinimized())
    return;

  for (auto& e : window.GetEventManager()) {
    EventDispatcher dispatcher(*e);

    dispatcher.Dispatch<WindowResizeEvent>([&](const WindowResizeEvent& event) {
      vw.Resize(event.width, event.height);
      return false;
    });

    dispatcher.Dispatch<WindowCloseEvent>([&](const WindowCloseEvent&) {
      if (id == 0) {
        app.Close();
      } else {
        mPendingClose.push_back(id);
      }
      return true;
    });

    layers.OnEvent(*e);
    // Note: Application-level global layer and view event dispatching
    // should probably happen in Application::Run or some other unified way.
    // For now, we'll let Application handle its own event distribution.
    app.OnAppEvent(*e);
  }
  window.GetEventManager().Clear();

  for (auto& layer : layers)
    layer->OnUpdate(dt);
  for (auto& layer : layers)
    layer->OnGuiRender();
}

void WindowManager::DrawWindow(WindowId id, WindowData& data, ViewManager& viewManager) {
  SD_ASSERT(data.render, "Render window must be valid");

  VulkanWindow& vw = *data.render;
  LayerList& layers = data.viewLayers;
  auto& app = Application::Get();
  auto& renderer = app.GetRenderer();

  if (vw.IsMinimized())
    return;

  if (vw.IsFramebufferResized()) {
    vw.RecreateSwapchain(layers);
    vw.ResetFramebufferResized();
  }

  // Acquire Image
  auto& frameSync = vw.GetFrameSync();
  auto acquireRes = vw.GetVulkanImages(frameSync.imageAcquired);

  if (!acquireRes) {
    if (acquireRes.error() == vk::Result::eErrorOutOfDateKHR) {
      vw.RecreateSwapchain(layers);
    }
    return;
  }
  u32 imageIndex = *acquireRes;

  vk::CommandBuffer cmd = renderer.BeginCommandBuffer(vw);

  // 1. Offscreen viewports (Handled by ViewManager)
  viewManager.RenderViews(cmd);

  // 2. Begin Main Render Pass
  renderer.BeginRenderPass(vw);

  // 3. Render Layers
  for (auto& layer : layers)
    layer->OnRender(cmd);

  if (id == 0) {
    app.GetImGuiContext().RenderDrawData(cmd);
  }

  vk::Result presentRes = renderer.EndFrame(vw);
  if (presentRes == vk::Result::eErrorOutOfDateKHR || presentRes == vk::Result::eSuboptimalKHR) {
    vw.RecreateSwapchain(layers);
  }
}

} // namespace SD

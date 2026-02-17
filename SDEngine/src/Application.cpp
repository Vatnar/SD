#include "Application.hpp"

#include "Core/Events/app/AppEvents.hpp"
#include "Layers/DebugInfoLayer.hpp"
#include "Layers/ImGuiLayer.hpp"
#include "Layers/PerformanceLayer.hpp"
#include "Layers/Shader2DLayer.hpp"

namespace SD {
Application::Application() :
  mVulkanCtx(mGlfwCtx), mRenderer(mVulkanCtx), mMainWindow(nullptr), mVulkanWindows(),
  mAppEventManager(), mNonVisualLayers() {
  auto window = WindowBuilder().SetTitle("SDEngine").SetSize(1280, 720).Build();

  mVulkanCtx.Init(*window);

  auto vw = std::make_unique<VulkanWindow>(*window, mVulkanCtx);
  mVulkanWindows.push_back(std::move(vw));

  mWindows.push_back(std::move(window));
  mMainWindow = mWindows.front().get();

  // Scenes, TODO: make it possible to run these, while reloading layers
  mGameScene = std::make_unique<Scene>();

  // Layers
  std::vector<std::string> textures = {"assets/textures/CreateNoteInteraction.png",
                                       "assets/textures/example.jpg"};

  // Note: Passing *mVulkanWindows.front() (which is the unique_ptr content)
  mMainWindow->LayerStack.PushLayer<Shader2DLayer>(*mGameScene, mVulkanCtx, *mVulkanWindows.front(),
                                                   textures);

  // dont need a window, just prints to terminal
  mNonVisualLayers.PushLayer<PerformanceLayer>(*mGameScene);


  mMainWindow->LayerStack.PushLayer<DebugInfoLayer>(*mGameScene);
  mMainWindow->LayerStack.PushLayer<ImGuiLayer>(*mGameScene, mVulkanCtx, *mMainWindow);
}
void Application::RenderWindow(Window& window, VulkanWindow& vw) {
  if (vw.IsMinimized())
    return;

  // 1. Process Window Events (including Resize)
  for (auto& e : window.GetEventManager()) {
    EventDispatcher dispatcher(*e);

    dispatcher.Dispatch<WindowResizeEvent>([&](const WindowResizeEvent& event) {
      vw.Resize(event.width, event.height);
      return false; // pass to layers too
    });
    dispatcher.Dispatch<WindowCloseEvent>([&](const WindowCloseEvent& ev) {
      if (&window == mMainWindow) {
        mRunning = false;
      }
      return false;
    });

    // propagate events
    window.LayerStack.OnEvent(*e);
  }
  window.GetEventManager().Clear();

  // 2. Handle Resize (Immediate)
  if (vw.IsFramebufferResized()) {
    vw.RecreateSwapchain(window.LayerStack);
    vw.ResetFramebufferResized();
  }

  if (!mRunning)
    return;

  // 3. Acquire Image
  auto& frameSync = vw.GetFrameSync();
  auto [acquireResult, imageIndex] = vw.GetVulkanImages(frameSync.imageAcquired);

  if (acquireResult == vk::Result::eErrorOutOfDateKHR) {
    vw.RecreateSwapchain(window.LayerStack);
    return;
  }

  // 4. ImGui Begin
  if (auto* imguiLayer = window.LayerStack.Get<ImGuiLayer>()) {
    imguiLayer->Begin();
  }

  // 5. Update Layers
  for (auto& layer : window.LayerStack) {
    if (dynamic_cast<ImGuiLayer*>(layer.get()))
      continue;
    layer->OnUpdate(0.016f);
  }

  // 6. Record & Submit
  vk::CommandBuffer cmd = mRenderer.BeginFrame(vw);
  for (auto& layer : window.LayerStack) {
    layer->OnRender(cmd);
  }
  vk::Result presentRes = mRenderer.EndFrame(vw); // Submit & Present

  if (presentRes == vk::Result::eErrorOutOfDateKHR || presentRes == vk::Result::eSuboptimalKHR) {
    vw.RecreateSwapchain(window.LayerStack);
  }
}
void Application::Run() {
  while (mRunning) {
    glfwPollEvents();

    for (auto& e : mAppEventManager) {
      OnAppEvent(*e);
    }
    mAppEventManager.Clear();

    for (auto& layer : mNonVisualLayers) {
      layer->OnUpdate(0.016f); // TODO: DeltaTime // fixed dt
    }

    // Process Windows
    for (size_t i = 0; i < mWindows.size(); ++i) {
      RenderWindow(*mWindows[i], *mVulkanWindows[i]);
    }
  }
}
void Application::OnAppEvent(Event& e) {
  EventDispatcher dispatcher(e);

  dispatcher.Dispatch<AppTerminateEvent>([this](AppTerminateEvent&) {
    mRunning = false;
    return true;
  });

  mNonVisualLayers.OnEvent(e);
}
} // namespace SD

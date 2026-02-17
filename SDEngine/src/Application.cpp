#include "Application.hpp"

#include <ranges>

#include "Core/Events/app/AppEvents.hpp"
#include "Core/Layers/PerformanceLayer.hpp"
#include "Core/Logging.hpp"
#include "Core/SDImGuiContext.hpp"
#include "Core/SceneView.hpp"

namespace SD {

Application* Application::sInstance = nullptr;
ImVec4 ApplyTheme() {
  ImGui::StyleColorsDark();
  ImGuiStyle& style = ImGui::GetStyle();
  ImVec4* colors = style.Colors;

  // --- Unified Palette ---
  const ImVec4 baseBlack = ImVec4(0.05f, 0.05f, 0.07f, 1.00f);
  const ImVec4 deepGrey = ImVec4(0.09f, 0.09f, 0.11f, 1.00f);     // Window headers
  const ImVec4 midGrey = ImVec4(0.14f, 0.14f, 0.17f, 1.00f);      // Widgets
  const ImVec4 accentIndigo = ImVec4(0.18f, 0.22f, 0.35f, 1.00f); // Selection
  const ImVec4 accentHover = ImVec4(0.24f, 0.28f, 0.45f, 1.00f);

  // --- Viewport & Background Consistency ---
  colors[ImGuiCol_WindowBg] = baseBlack;
  colors[ImGuiCol_ChildBg] = baseBlack;
  colors[ImGuiCol_PopupBg] = baseBlack;

  // This ensures detached windows don't revert to default grey/blue
  colors[ImGuiCol_TitleBg] = deepGrey;
  colors[ImGuiCol_TitleBgActive] = midGrey;
  colors[ImGuiCol_TitleBgCollapsed] = baseBlack;
  colors[ImGuiCol_MenuBarBg] = deepGrey;

  // --- Docking Visuals ---
  colors[ImGuiCol_DockingPreview] = ImVec4(accentIndigo.x, accentIndigo.y, accentIndigo.z, 0.70f);
  colors[ImGuiCol_DockingEmptyBg] = baseBlack;

  // --- Nav & Tabs (Unified with Palette) ---
  colors[ImGuiCol_Header] = accentIndigo;
  colors[ImGuiCol_HeaderHovered] = accentHover;
  colors[ImGuiCol_HeaderActive] = accentIndigo;

  colors[ImGuiCol_Tab] = deepGrey;
  colors[ImGuiCol_TabHovered] = accentHover;
  colors[ImGuiCol_TabActive] = accentIndigo;
  colors[ImGuiCol_TabUnfocused] = deepGrey;
  colors[ImGuiCol_TabUnfocusedActive] = deepGrey;

  // --- Widgets ---
  colors[ImGuiCol_FrameBg] = midGrey;
  colors[ImGuiCol_FrameBgHovered] = accentIndigo;
  colors[ImGuiCol_FrameBgActive] = accentHover;

  colors[ImGuiCol_Button] = midGrey;
  colors[ImGuiCol_ButtonHovered] = accentHover;
  colors[ImGuiCol_ButtonActive] = accentIndigo;

  // --- Sharpness & Geometry ---
  style.WindowRounding = 0.0f;
  style.ChildRounding = 0.0f;
  style.FrameRounding = 0.0f;
  style.PopupRounding = 0.0f;
  style.ScrollbarRounding = 0.0f;
  style.GrabRounding = 0.0f;
  style.TabRounding = 0.0f;

  style.WindowBorderSize = 1.0f;
  style.FrameBorderSize = 0.0f;
  style.PopupBorderSize = 1.0f;

  // --- Spacing ---
  style.WindowPadding = ImVec2(8, 8);
  style.FramePadding = ImVec2(6, 4);
  style.ItemSpacing = ImVec2(8, 6);

  // This is crucial for keeping windows from "popping" when dragged
  style.DisplaySafeAreaPadding = ImVec2(0, 0);

  return baseBlack;
}
Application::Application(const ApplicationSpecification& spec) :
  mGlfwCtx(std::make_unique<GlfwContext>()), mVulkanCtx(std::make_unique<VulkanContext>(*mGlfwCtx)),
  mImGuiCtx(std::make_unique<SDImGuiContext>()), mSpec(spec) {
  sInstance = this;

  WindowProps props(spec.name, spec.width, spec.height);
  auto window =
      WindowBuilder().SetTitle(props.title.c_str()).SetSize(props.width, props.height).Build();
  mVulkanCtx->Init(*window);

  // 4. Create Render Window
  auto vw = std::make_unique<VulkanWindow>(*window, *mVulkanCtx);

  // 5. Init ImGui for Main Window
  // 5. Init ImGui Context (Global)
  mImGuiCtx->Init(*window, *vw);
  // style imgui to be more professional
  ImVec4 clearColor = ApplyTheme();
  ImGuiIO& io = ImGui::GetIO();
  io.Fonts->AddFontFromFileTTF("assets/fonts/Inter_18pt-Regular.ttf", 15.0f);


  // 6. Store in Map
  WindowId id = mNextWindowId++;
  WindowData data;
  data.logic = std::move(window);
  data.render = std::move(vw);
  mWindows[id] = std::move(data);

  // 7. Init Renderer
  mRenderer = std::make_unique<VulkanRenderer>(*mVulkanCtx);
  mRenderer->SetClearColor({clearColor.x, clearColor.y, clearColor.z, clearColor.w});
}

Application::~Application() {
  // Ensure GPU is idle before destroying resources
  if (mVulkanCtx) {
    (void)mVulkanCtx.get()->GetVulkanDevice()->waitIdle();
  }
  mWindows.clear(); // Destroy windows before Context
}

WindowId Application::CreateWindow(const WindowProps& props) {
  WindowId id = mNextWindowId++;

  // 1. Create Logic Window (GLFW)
  auto window =
      WindowBuilder().SetTitle(props.title.c_str()).SetSize(props.width, props.height).Build();

  // 2. Ensure Vulkan Context is initialized (needs a surface usually)
  if (!mVulkanCtx->IsInitialized()) {
    mVulkanCtx->Init(*window);
  }

  // 3. Create Render Window (Swapchain/Surface)
  auto vw = std::make_unique<VulkanWindow>(*window, *mVulkanCtx);


  // 5. Store in Map
  WindowData data;
  data.logic = std::move(window);
  data.render = std::move(vw);


  mWindows[id] = std::move(data);


  return id;
}

Window& Application::GetWindow(WindowId id) {
  if (mWindows.find(id) != mWindows.end()) {
    return *mWindows.at(id).logic;
  }
  Abort("Invalid Window ID");
}

VulkanWindow& Application::GetRenderWindow(WindowId id) {
  if (mWindows.find(id) != mWindows.end()) {
    return *mWindows.at(id).render;
  }
  Abort("Invalid Window ID");
}
void Application::DestroyWindow(WindowId id) {
  (void)mVulkanCtx->GetVulkanDevice()->waitIdle();
  mWindows.erase(id);
}


// TODO: We want a better way for addressing views
void Application::Run() {
  mLastTime = glfwGetTime();

  while (mRunning) {
    double currentTime = glfwGetTime();
    double frameTime = currentTime - mLastTime;
    mLastTime = currentTime;

    if (frameTime > 0.25)
      frameTime = 0.25;
    mAccumulator += frameTime;
    glfwPollEvents();

    // 1. Process Global App Events
    for (auto& e : mAppEventManager) {
      OnAppEvent(*e);
    }
    mAppEventManager.Clear();

    int maxSteps = 5;
    while (mAccumulator >= mFixedTimeStep && maxSteps > 0) {
      // NOTE: FIXED UPDATE
      for (auto& layer : mGlobalLayers)
        layer->OnFixedUpdate(mFixedTimeStep);
      mAccumulator -= mFixedTimeStep;
      maxSteps--;
    }

    // 2. Begin ImGui Frame
    mImGuiCtx->BeginFrame();
    mImGuiCtx->BeginDockSpace(mSpec.name);

    // 3. Update
    for (auto& layer : mGlobalLayers) {
      layer->OnUpdate(static_cast<float>(frameTime));
      layer->OnGuiRender();
    }

    for (auto& [id, data] : mWindows) {
      UpdateWindow(id, data, static_cast<float>(frameTime));
    }

    for (auto it = mViewsById.begin(); it != mViewsById.end();) {
      if (!it->second->IsOpen()) {
        it = mViewsById.erase(it);
      } else {
        it->second->OnUpdate(static_cast<float>(frameTime));
        it->second->OnGuiRender();
        ++it;
      }
    }

    mImGuiCtx->EndDockSpace();

    // 4. End ImGui Frame
    mImGuiCtx->EndFrame();

    // 5. Draw
    for (auto& [id, data] : mWindows) {
      DrawWindow(id, data);
    }

    // 6. Viewports
    mImGuiCtx->UpdatePlatformWindows();

    for (WindowId id : mPendingClose) {
      DestroyWindow(id);
    }
    mPendingClose.clear();
  }
}

void Application::UpdateWindow(WindowId id, WindowData& data, float dt) {
  Window& window = *data.logic;
  VulkanWindow& vw = *data.render;
  LayerList& views = data.viewLayers;

  if (vw.IsMinimized())
    return;

  // --- Event Processing ---
  for (auto& e : window.GetEventManager()) {
    EventDispatcher dispatcher(*e);

    // Hardware Resize
    dispatcher.Dispatch<WindowResizeEvent>([&](const WindowResizeEvent& event) {
      vw.Resize(event.width, event.height);
      return false; // Propagate to layers
    });

    // Window Close
    dispatcher.Dispatch<WindowCloseEvent>([&](const WindowCloseEvent&) {
      if (id == 0) {
        mRunning = false;
      } else {
        mPendingClose.push_back(id);
      }
      return true;
    });

    views.OnEvent(*e);
  }
  window.GetEventManager().Clear();

  for (auto& layer : views)
    layer->OnUpdate(dt);
  for (auto& layer : views)
    layer->OnGuiRender();
}

void Application::DrawWindow(WindowId id, WindowData& data) {
  VulkanWindow& vw = *data.render;
  LayerList& views = data.viewLayers;

  if (vw.IsMinimized())
    return;

  // --- Swapchain Management ---
  if (vw.IsFramebufferResized()) {
    vw.RecreateSwapchain(views);
    vw.ResetFramebufferResized();
  }

  if (!mRunning)
    return;

  // --- Acquire Image ---
  auto& frameSync = vw.GetFrameSync();
  auto acquireRes = vw.GetVulkanImages(frameSync.imageAcquired);

  if (!acquireRes) {
    if (acquireRes.error() == vk::Result::eErrorOutOfDateKHR) {
      vw.RecreateSwapchain(views);
    }
    return;
  }
  u32 imageIndex = *acquireRes;

  vk::CommandBuffer cmd = mRenderer->BeginCommandBuffer(vw);

  // 1. Pre-render offscreen viewports
  for (auto& [viewId, view] : mViewsById) {
    if (view->IsOpen()) {
      view->OnRender(cmd);
    }
  }

  // 2. Begin Main Render Pass (Swapchain)
  mRenderer->BeginRenderPass(vw);

  // 3. Render Layers & Global Overlays
  for (auto& layer : views)
    layer->OnRender(cmd);

  if (id == 0) {
    mImGuiCtx->RenderDrawData(cmd);
  }

  // 4. Submit & Present
  vk::Result presentRes = mRenderer->EndFrame(vw);
  if (presentRes == vk::Result::eErrorOutOfDateKHR || presentRes == vk::Result::eSuboptimalKHR) {
    vw.RecreateSwapchain(views);
  }
}


void Application::OnAppEvent(Event& e) {
  EventDispatcher dispatcher(e);
  dispatcher.Dispatch<AppTerminateEvent>([this](AppTerminateEvent&) {
    mRunning = false;
    return true;
  });

  mGlobalLayers.OnEvent(e);
}

} // namespace SD

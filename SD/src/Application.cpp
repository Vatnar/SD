#include "Application.hpp"

#include <dlfcn.h>
#include <filesystem>
#include <ranges>

#include "Core/Events/app/AppEvents.hpp"
#include "Core/Layers/PerformanceLayer.hpp"
#include "Core/Logging.hpp"
#include "Core/SDImGuiContext.hpp"

namespace SD {

Application* Application::sInstance = nullptr;

ImVec4 ApplyTheme() {
  ImGui::StyleColorsDark();
  ImGuiStyle& style = ImGui::GetStyle();
  ImVec4* colors = style.Colors;

  const ImVec4 baseBlack = ImVec4(0.05f, 0.05f, 0.07f, 1.00f);
  const ImVec4 deepGrey = ImVec4(0.09f, 0.09f, 0.11f, 1.00f);
  const ImVec4 midGrey = ImVec4(0.14f, 0.14f, 0.17f, 1.00f);
  const ImVec4 accentIndigo = ImVec4(0.18f, 0.22f, 0.35f, 1.00f);
  const ImVec4 accentHover = ImVec4(0.24f, 0.28f, 0.45f, 1.00f);

  colors[ImGuiCol_WindowBg] = baseBlack;
  colors[ImGuiCol_ChildBg] = baseBlack;
  colors[ImGuiCol_PopupBg] = baseBlack;
  colors[ImGuiCol_TitleBg] = deepGrey;
  colors[ImGuiCol_TitleBgActive] = midGrey;
  colors[ImGuiCol_TitleBgCollapsed] = baseBlack;
  colors[ImGuiCol_MenuBarBg] = deepGrey;
  colors[ImGuiCol_DockingPreview] = ImVec4(accentIndigo.x, accentIndigo.y, accentIndigo.z, 0.70f);
  colors[ImGuiCol_DockingEmptyBg] = baseBlack;
  colors[ImGuiCol_Header] = accentIndigo;
  colors[ImGuiCol_HeaderHovered] = accentHover;
  colors[ImGuiCol_HeaderActive] = accentIndigo;
  colors[ImGuiCol_Tab] = deepGrey;
  colors[ImGuiCol_TabHovered] = accentHover;
  colors[ImGuiCol_TabActive] = accentIndigo;
  colors[ImGuiCol_TabUnfocused] = deepGrey;
  colors[ImGuiCol_TabUnfocusedActive] = deepGrey;
  colors[ImGuiCol_FrameBg] = midGrey;
  colors[ImGuiCol_FrameBgHovered] = accentIndigo;
  colors[ImGuiCol_FrameBgActive] = accentHover;
  colors[ImGuiCol_Button] = midGrey;
  colors[ImGuiCol_ButtonHovered] = accentHover;
  colors[ImGuiCol_ButtonActive] = accentIndigo;

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
  style.WindowPadding = ImVec2(8, 8);
  style.FramePadding = ImVec2(6, 4);
  style.ItemSpacing = ImVec2(8, 6);
  style.DisplaySafeAreaPadding = ImVec2(0, 0);

  return baseBlack;
}

Application::Application(const ApplicationSpecification& spec, RuntimeStateManager* stateManager) :

  mSpec(spec), mHotReloadEnabled(spec.enableHotReload), mGlfwCtx(std::make_unique<GlfwContext>()),
  mVulkanCtx(std::make_unique<VulkanContext>(*mGlfwCtx)),
  mImGuiCtx(std::make_unique<SDImGuiContext>()), mWindowManager(std::make_unique<WindowManager>()),
  mViewManager(std::make_unique<ViewManager>()), mStateManager(stateManager) {
  sInstance = this;

  WindowProps props(spec.name, spec.width, spec.height);
  WindowId mainId = mWindowManager->Create(props);
  auto& window = mWindowManager->GetWindow(mainId);
  auto& vw = mWindowManager->GetRenderWindow(mainId);

  mImGuiCtx->Init(window, vw);
  ImVec4 clearColor = ApplyTheme();

  const char* fontPath = "assets/engine/fonts/Inter_18pt-Regular.ttf";
  if (std::filesystem::exists(fontPath)) {
    ImGui::GetIO().Fonts->AddFontFromFileTTF(fontPath, 15.0f);
  } else {
    SPDLOG_WARN("Could not find font file: assets/fonts/Inter_18pt-Regular.ttf. Falling back to "
                "default font.");
  }

  // 3. Init Renderer
  mRenderer = std::make_unique<VulkanRenderer>(*mVulkanCtx);
  mRenderer->SetClearColor({clearColor.x, clearColor.y, clearColor.z, clearColor.w});
}

Application::~Application() {
  if (mVulkanCtx) {
    (void)mVulkanCtx->GetVulkanDevice()->waitIdle();
  }
  mWindowManager.reset();
}

void Application::Run(std::atomic<bool>* externalStop) {
  while (mRunning) {
    if (externalStop)
      mRunning = !externalStop->load(std::memory_order_acquire);
    Frame();
  }
}

void Application::Frame() {
  mTimer.Begin();
  glfwPollEvents();
  mTimer.BeginWork();

  float dt = mTimer.GetFrameTime();

  // 1. Events
  for (auto& e : mAppEventManager) {
    OnAppEvent(*e);
  }
  mAppEventManager.Clear();

  // 2. Fixed Update
  while (mTimer.ConsumeFixedStep()) {
    for (auto& layer : mGlobalLayers)
      layer->OnFixedUpdate(mTimer.GetFixedTimeStep());
  }

  // 3. ImGui Frame
  mImGuiCtx->BeginFrame();
  mImGuiCtx->BeginDockSpace(mSpec.name);

  if (ImGui::BeginMainMenuBar()) {
    if (ImGui::BeginMenu("File")) {
      if (ImGui::MenuItem("Exit", "Alt+F4"))
        mRunning = false;
      ImGui::EndMenu();
    }
    mGlobalLayers.OnImGuiMenuBar();
    for (auto& [id, data] : mWindowManager->GetWindows())
      data.viewLayers.OnImGuiMenuBar();
    for (auto& [id, view] : mViewManager->GetViews())
      view->GetLayers().OnImGuiMenuBar();
    ImGui::EndMainMenuBar();
  }

  // 4. Update
  for (auto& layer : mGlobalLayers) {
    layer->OnUpdate(dt);
    layer->OnGuiRender();
  }

  mWindowManager->UpdateWindows(dt);
  mViewManager->UpdateViews(dt);

  mImGuiCtx->EndDockSpace();
  mImGuiCtx->EndFrame();

  // 5. Hot Reload Polling
  if (mHotReloadEnabled) {
    mHotReloadTimer += dt;
    if (mHotReloadTimer >= 0.5f) {
      mHotReloadTimer = 0.0f;
      auto changed = mRenderer->GetShaderLibrary().CheckForChanges();
      if (!changed.empty()) {
        SPDLOG_INFO("Shader changes detected: {}. Reloading...", changed.size());
        ReloadShaders();
      }
    }
  }

  // 6. Render
  mWindowManager->DrawWindows(*mViewManager);

  mTimer.EndWork();

  // 7. Viewports
  mImGuiCtx->UpdatePlatformWindows();
  mWindowManager->ProcessPendingCloses();
  mViewManager->CleanupClosedViews();
}

void Application::OnAppEvent(Event& e) {
  EventDispatcher dispatcher(e);
  dispatcher.Dispatch<AppTerminateEvent>([this](AppTerminateEvent&) {
    mRunning = false;
    return true;
  });

  mGlobalLayers.OnEvent(e);
}

Scene* Application::CreateScene(const std::string& name) {
  for (auto& scene : mScenes) {
    if (scene->GetName() == name) {
      SPDLOG_WARN("Scene '{}' already exists, returning existing scene", name);
      return scene.get();
    }
  }
  auto scene = std::make_unique<Scene>(name);
  Scene* ptr = scene.get();
  mScenes.push_back(std::move(scene));
  return ptr;
}

Scene* Application::GetScene(const std::string& name) {
  for (auto& scene : mScenes) {
    if (scene->GetName() == name) {
      return scene.get();
    }
  }
  return nullptr;
}

std::vector<Scene*> Application::GetScenes() {
  std::vector<Scene*> scenes;
  for (auto& scene : mScenes) {
    scenes.push_back(scene.get());
  }
  auto addUnique = [&](Scene* s) {
    if (s && std::find(scenes.begin(), scenes.end(), s) == scenes.end())
      scenes.push_back(s);
  };

  for (auto& layer : mGlobalLayers)
    addUnique(layer->GetScene());
  for (auto& [id, data] : mWindowManager->GetWindows()) {
    for (auto& layer : data.viewLayers)
      addUnique(layer->GetScene());
  }
  auto viewScenes = mViewManager->GetScenes();
  for (auto* s : viewScenes) {
    addUnique(s);
  }
  return scenes;
}

void Application::SetGameContext(GameContext* ctx) {
  mGameContext = ctx;
}

void Application::ClearGameLayers() {
  try {
    mGlobalLayers.Clear();
    mViewManager->Clear();
    mScenes.clear();
  } catch (const std::exception& e) {
    SPDLOG_ERROR("Exception in ClearGameLayers: {}", e.what());
  }
}

void Application::ReloadGame() {
  SPDLOG_INFO("Starting game reload...");
  try {
    if (mGameContext) {
      mGameContext->OnUnload();
      delete mGameContext;
      mGameContext = nullptr;
    }

    ClearGameLayers();

    // Close old handle BEFORE opening new one, so dlopen loads fresh code
    if (mGameHandle) {
      dlclose(mGameHandle);
      mGameHandle = nullptr;
    }

    void* handle = dlopen("libSandboxApp.so", RTLD_NOW | RTLD_GLOBAL);
    if (!handle) {
      SPDLOG_ERROR("Failed to reload game DLL: {}", dlerror());
      return;
    }
    mGameHandle = handle;

    using CreateGameFn = GameContext* (*)(RuntimeStateManager*);
    auto* create = reinterpret_cast<CreateGameFn>(dlsym(handle, "CreateGame"));
    if (!create) {
      SPDLOG_ERROR("Failed to find CreateGame: {}", dlerror());
      dlclose(handle);
      return;
    }

    mGameContext = create(mStateManager);
    mGameContext->OnLoad(*this);

    mStateManager->Restore(this);

    SPDLOG_INFO("Game reloaded successfully");
  } catch (const std::exception& e) {
    SPDLOG_ERROR("Exception during reload: {}", e.what());
  }
}

void Application::ReloadShaders() {
  (void)mVulkanCtx->GetVulkanDevice()->waitIdle();
  mRenderer->GetShaderLibrary().ClearCache();
  mRenderer->GetPipelineFactory().RecreateAllPipelines();
}

} // namespace SD

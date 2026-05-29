#include "Application.hpp"

#include <algorithm>
#include <dlfcn.h>
#include <filesystem>

#include "Core/Events/app/AppEvents.hpp"
#include "Core/Layers/PerformanceLayer.hpp"
#include "Core/LayoutManager.hpp"
#include "Core/SDImGuiContext.hpp"
#include "Core/Vulkan/VulkanRenderer.hpp"
#include "GameContext.hpp"
#include "RuntimeStateManager.hpp"

namespace sd {

Application* Application::s_instance = nullptr;

ImVec4 apply_theme() {
  ImGui::StyleColorsDark();
  ImGuiStyle& style = ImGui::GetStyle();
  ImVec4* colors = style.Colors;

  constexpr auto base_black = ImVec4(0.05f, 0.05f, 0.07f, 1.00f);
  constexpr auto deep_grey = ImVec4(0.09f, 0.09f, 0.11f, 1.00f);
  constexpr auto mid_grey = ImVec4(0.14f, 0.14f, 0.17f, 1.00f);
  constexpr auto accent_indigo = ImVec4(0.18f, 0.22f, 0.35f, 1.00f);
  constexpr auto accent_hover = ImVec4(0.24f, 0.28f, 0.45f, 1.00f);

  colors[ImGuiCol_WindowBg] = base_black;
  colors[ImGuiCol_ChildBg] = base_black;
  colors[ImGuiCol_PopupBg] = base_black;
  colors[ImGuiCol_TitleBg] = deep_grey;
  colors[ImGuiCol_TitleBgActive] = mid_grey;
  colors[ImGuiCol_TitleBgCollapsed] = base_black;
  colors[ImGuiCol_MenuBarBg] = deep_grey;
  colors[ImGuiCol_DockingPreview] = ImVec4(accent_indigo.x, accent_indigo.y, accent_indigo.z, 0.70f);
  colors[ImGuiCol_DockingEmptyBg] = base_black;
  colors[ImGuiCol_Header] = accent_indigo;
  colors[ImGuiCol_HeaderHovered] = accent_hover;
  colors[ImGuiCol_HeaderActive] = accent_indigo;
  colors[ImGuiCol_Tab] = deep_grey;
  colors[ImGuiCol_TabHovered] = accent_hover;
  colors[ImGuiCol_TabActive] = accent_indigo;
  colors[ImGuiCol_TabUnfocused] = deep_grey;
  colors[ImGuiCol_TabUnfocusedActive] = deep_grey;
  colors[ImGuiCol_FrameBg] = mid_grey;
  colors[ImGuiCol_FrameBgHovered] = accent_indigo;
  colors[ImGuiCol_FrameBgActive] = accent_hover;
  colors[ImGuiCol_Button] = mid_grey;
  colors[ImGuiCol_ButtonHovered] = accent_hover;
  colors[ImGuiCol_ButtonActive] = accent_indigo;

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

  return base_black;
}

Application::Application(const ApplicationSpecification& spec, RuntimeStateManager* state_manager) :

  m_spec(spec), m_hot_reload_enabled(spec.enableHotReload), m_glfw_ctx(std::make_unique<GlfwContext>()),
  m_vulkan_ctx(std::make_unique<VulkanContext>(*m_glfw_ctx)),
  m_im_gui_ctx(std::make_unique<SDImGuiContext>()), m_window_manager(std::make_unique<WindowManager>()),
  m_view_manager(std::make_unique<ViewManager>()), m_layout_manager(std::make_unique<LayoutManager>()),
  m_state_manager(state_manager) {
  s_instance = this;

  WindowProps props(spec.name, spec.width, spec.height);
  WindowId main_id = m_window_manager->create(props);
  auto& window = m_window_manager->get_window(main_id);
  auto& vw = m_window_manager->get_render_window(main_id);

  m_im_gui_ctx->init(window, vw);
  ImVec4 clear_color = apply_theme();

  const char* font_path = "assets/engine/fonts/Inter_18pt-Regular.ttf";
  if (std::filesystem::exists(font_path)) {
    ImGui::GetIO().Fonts->AddFontFromFileTTF(font_path, 15.0f);
  } else {
    log::engine::warn(
        "Could not find font file: assets/fonts/Inter_18pt-Regular.ttf. Falling back to "
        "default font.");
  }

  m_renderer = std::make_unique<VulkanRenderer>(*m_vulkan_ctx);
  m_renderer->set_clear_color({clear_color.x, clear_color.y, clear_color.z, clear_color.w});

  m_layout_manager->init();
}

Application::~Application() {
  // Wait for GPU to finish all work
  if (m_vulkan_ctx && m_vulkan_ctx->get_vulkan_device()) {
    (void)m_vulkan_ctx->get_vulkan_device()->waitIdle();
  }

  // Clear all layers FIRST (before destroying windows/views)
  // This ensures OnDetach is called while resources are still valid
  m_global_layers.clear();
  if (m_view_manager) {
    m_view_manager->clear();
  }
  m_scenes.clear();

  m_window_manager.reset();

  m_im_gui_ctx.reset();

  m_renderer.reset();

  m_vulkan_ctx.reset();
}

void Application::run(const std::atomic<bool>* external_stop) {
  while (m_is_running) {
    if (external_stop)
      m_is_running = !external_stop->load(std::memory_order_acquire);
    frame();
  }
}

void Application::frame() {
  m_timer.begin();
  glfwPollEvents();
  m_timer.begin_work();

  float dt = m_timer.get_frame_time();

  for (auto& e : m_app_event_manager) {
    on_app_event(*e);
  }
  m_app_event_manager.clear();

  while (m_timer.consume_fixed_step()) {
    for (auto& layer : m_global_layers)
      layer->on_fixed_update(m_timer.get_fixed_time_step());
  }

  m_im_gui_ctx->begin_frame();
  m_im_gui_ctx->begin_dock_space(m_spec.name);

  if (!ImGui::GetCurrentContext())
    return;
  if (ImGui::BeginMainMenuBar()) {
    if (ImGui::BeginMenu("File")) {
      if (ImGui::MenuItem("Exit", "Alt+F4"))
        m_is_running = false;
      ImGui::EndMenu();
    }
    m_global_layers.on_imGui_menu_bar();
    for (auto& data : m_window_manager->get_windows() | std::views::values)
      data.view_layers.on_imGui_menu_bar();
    for (auto& view : m_view_manager->get_views() | std::views::values)
      view->get_layers().on_imGui_menu_bar();
    ImGui::EndMainMenuBar();
  }

  for (auto& layer : m_global_layers) {
    layer->on_update(dt);
    layer->on_gui_render();
  }

  m_window_manager->update_windows(dt);
  m_view_manager->update_views(dt);

  m_im_gui_ctx->end_dock_space();
  m_im_gui_ctx->end_frame();

  if (m_hot_reload_enabled) {
    m_hot_reload_timer += dt;
    if (m_hot_reload_timer >= 0.5f) {
      m_hot_reload_timer = 0.0f;
      auto changed = m_renderer->get_shader_library().check_for_changes();
      if (!changed.empty()) {
        log::engine::info("Shader changes detected: {}. Reloading...", changed.size());
        reload_shaders();
      }
    }
  }
  m_window_manager->draw_windows(*m_view_manager);

  m_timer.end_work();

  m_im_gui_ctx->update_platform_windows();
  m_window_manager->process_pending_closes();
  m_view_manager->cleanup_closed_views();
}

void Application::on_app_event(Event& e) {
  EventDispatcher dispatcher(e);
  dispatcher.dispatch<AppTerminateEvent>([this](AppTerminateEvent&) {
    m_is_running = false;
    return true;
  });

  m_global_layers.on_event(e);
}

Scene* Application::create_scene(const std::string& name) {
  for (auto& scene : m_scenes) {
    if (scene->get_name() == name) {
      log::engine::warn("Scene '{}' already exists, returning existing scene", name);
      return scene.get();
    }
  }
  auto scene = std::make_unique<Scene>(name);
  m_scenes.push_back(std::move(scene));

  return m_scenes.back().get();
}

Scene* Application::get_scene(const std::string& name) const {
  for (auto& scene : m_scenes) {
    if (scene->get_name() == name) {
      return scene.get();
    }
  }
  return nullptr;
}

std::vector<Scene*> Application::get_scenes() {
  std::vector<Scene*> scenes;
  for (auto& scene : m_scenes) {
    scenes.push_back(scene.get());
  }
  auto add_unique = [&](Scene* s) {
    if (s && std::ranges::find(scenes, s) == scenes.end())
      scenes.push_back(s);
  };

  for (auto& layer : m_global_layers)
    add_unique(layer->get_scene());
  for (auto& data : m_window_manager->get_windows() | std::views::values) {
    for (auto& layer : data.view_layers)
      add_unique(layer->get_scene());
  }
  auto view_scenes = m_view_manager->get_scenes();
  for (auto* s : view_scenes) {
    add_unique(s);
  }
  return scenes;
}

void Application::set_game_context(GameContext* ctx) {
  m_game_context = ctx;
}

void Application::clear_game_layers() {
  m_global_layers.clear();
  if (m_view_manager) {
    m_view_manager->clear();
  }
  m_scenes.clear();
}

void Application::reload_game() {
  log::engine::info("Starting game reload...");

  if (m_game_context) {
    m_game_context->on_unload();
    delete m_game_context;
    m_game_context = nullptr;
  }

  clear_game_layers();

  if (m_game_handle) {
    dlclose(m_game_handle);
    m_game_handle = nullptr;
  }

  void* handle = dlopen(m_spec.gameSoPath.c_str(), RTLD_NOW | RTLD_GLOBAL);
  if (!handle) {
    log::engine::error("Failed to reload game DLL: {}", dlerror());
    return;
  }
  m_game_handle = handle;

  using CreateGameFn = GameContext* (*)(RuntimeStateManager*);
  auto* create = reinterpret_cast<CreateGameFn>(dlsym(handle, "CreateGame"));
  if (!create) {
    log::engine::error("Failed to find CreateGame: {}", dlerror());
    dlclose(handle);
    return;
  }

  m_game_context = create(m_state_manager);
  if (!m_game_context) {
    log::engine::error("Game DLL returned null from CreateGame");
    dlclose(handle);
    m_game_handle = nullptr;
    return;
  }
  m_game_context->on_load(*this);

  m_state_manager->restore(this);

  log::engine::info("Game reloaded successfully");
}

void Application::reload_shaders() const {
  (void)m_vulkan_ctx->get_vulkan_device()->waitIdle();
  m_renderer->get_shader_library().ClearCache();
  auto failures = m_renderer->get_pipeline_factory().recreate_all_pipelines();
  if (!failures.empty()) {
    log::engine::warn("{} pipeline(s) failed to recreate during hot reload", failures.size());
  }
}

} // namespace SD

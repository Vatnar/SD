#include "SD/Application.hpp"

#include <filesystem>
#include <meta>

#include "SD/RuntimeStateManager.hpp"
#include "SD/core/LayoutManager.hpp"
#include "SD/core/SDImGuiContext.hpp"
#include "SD/core/events/app/app_events.hpp"
#include "SD/core/layers/PerformanceLayer.hpp"
#include "SD/core/vulkan/VulkanRenderer.hpp"


namespace sd {


ImVec4 apply_theme() {
  ImGui::StyleColorsDark();

  ImGuiStyle& style  = ImGui::GetStyle();
  ImVec4*     colors = style.Colors;

  constexpr auto base_black    = ImVec4(0.05f, 0.05f, 0.07f, 1.00f);
  constexpr auto deep_grey     = ImVec4(0.09f, 0.09f, 0.11f, 1.00f);
  constexpr auto mid_grey      = ImVec4(0.14f, 0.14f, 0.17f, 1.00f);
  constexpr auto accent_indigo = ImVec4(0.18f, 0.22f, 0.35f, 1.00f);
  constexpr auto accent_hover  = ImVec4(0.24f, 0.28f, 0.45f, 1.00f);

  colors[ImGuiCol_WindowBg]         = base_black;
  colors[ImGuiCol_ChildBg]          = base_black;
  colors[ImGuiCol_PopupBg]          = base_black;
  colors[ImGuiCol_TitleBg]          = deep_grey;
  colors[ImGuiCol_TitleBgActive]    = mid_grey;
  colors[ImGuiCol_TitleBgCollapsed] = base_black;
  colors[ImGuiCol_MenuBarBg]        = deep_grey;
  colors[ImGuiCol_DockingPreview] =
      ImVec4(accent_indigo.x, accent_indigo.y, accent_indigo.z, 0.70f);
  colors[ImGuiCol_DockingEmptyBg]     = base_black;
  colors[ImGuiCol_Header]             = accent_indigo;
  colors[ImGuiCol_HeaderHovered]      = accent_hover;
  colors[ImGuiCol_HeaderActive]       = accent_indigo;
  colors[ImGuiCol_Tab]                = deep_grey;
  colors[ImGuiCol_TabHovered]         = accent_hover;
  colors[ImGuiCol_TabActive]          = accent_indigo;
  colors[ImGuiCol_TabUnfocused]       = deep_grey;
  colors[ImGuiCol_TabUnfocusedActive] = deep_grey;
  colors[ImGuiCol_FrameBg]            = mid_grey;
  colors[ImGuiCol_FrameBgHovered]     = accent_indigo;
  colors[ImGuiCol_FrameBgActive]      = accent_hover;
  colors[ImGuiCol_Button]             = mid_grey;
  colors[ImGuiCol_ButtonHovered]      = accent_hover;
  colors[ImGuiCol_ButtonActive]       = accent_indigo;

  style.WindowRounding         = 0.0f;
  style.ChildRounding          = 0.0f;
  style.FrameRounding          = 0.0f;
  style.PopupRounding          = 0.0f;
  style.ScrollbarRounding      = 0.0f;
  style.GrabRounding           = 0.0f;
  style.TabRounding            = 0.0f;
  style.WindowBorderSize       = 1.0f;
  style.FrameBorderSize        = 0.0f;
  style.PopupBorderSize        = 1.0f;
  style.WindowPadding          = ImVec2(8, 8);
  style.FramePadding           = ImVec2(6, 4);
  style.ItemSpacing            = ImVec2(8, 6);
  style.DisplaySafeAreaPadding = ImVec2(0, 0);

  return base_black;
}

Application::Application(const ApplicationSpecification& spec, RuntimeStateManager* state_manager) :
  is_running(true), hot_reload_enabled(spec.enableHotReload), app_spec(spec),
  window_manager(nullptr), layout_manager(nullptr), scene_manager(), app_event_manager(),
  state_manager(state_manager), timer(), m_glfw_ctx(nullptr), m_vulkan_ctx(nullptr),
  m_renderer(nullptr), m_imgui_ctx(nullptr) {
  engine_arena = arena_alloc(ArenaParams{
      .name = "EngineArena",
  });
  Arena* a     = engine_arena;

  m_glfw_ctx = arena_push<GlfwContext>(a);
  new (m_glfw_ctx) GlfwContext();

  m_vulkan_ctx = arena_push<VulkanContext>(a);
  new (m_vulkan_ctx) VulkanContext(*m_glfw_ctx);

  m_renderer = arena_push<VulkanRenderer>(a);
  new (m_renderer) VulkanRenderer(*m_vulkan_ctx, timer);

  m_imgui_ctx = arena_push<SDImGuiContext>(a);
  new (m_imgui_ctx) SDImGuiContext(SDImGuiCallbacks{.close_app = [this] { close(); }});

  view_manager = arena_push<ViewManager>(a);
  new (view_manager) ViewManager();

  layout_manager = arena_push<LayoutManager>(a);
  new (layout_manager) LayoutManager();

  window_manager = arena_push<WindowManager>(a);
  new (window_manager) WindowManager(
      services(),
      WindowManagerCallbacks{.close_app    = [this] { close(); },
                             .on_app_event = [this](EventVariant& e) { on_app_event(e); }},
      a);

  WindowProps props(spec.name, spec.width, spec.height);
  WindowId    main_id = window_manager->create(props, a);
  auto&       window  = window_manager->get_window(main_id);
  auto&       vw      = window_manager->get_render_window(main_id);

  m_renderer->init();

  m_imgui_ctx->init(window, vw);
  ImVec4 clear_color = apply_theme();

  const char* font_path = "assets/engine/fonts/Inter_18pt-Regular.ttf";
  if (std::filesystem::exists(font_path)) {
    ImGui::GetIO().Fonts->AddFontFromFileTTF(font_path, 15.0f);
  } else {
    log::engine::warn(
        "Could not find font file: assets/fonts/Inter_18pt-Regular.ttf. Falling back to "
        "default font.");
  }

  m_renderer->set_clear_color({clear_color.x, clear_color.y, clear_color.z, clear_color.w});

  layout_manager->init();
}

Application::~Application() {
  // Wait for GPU to finish all work
  if (m_vulkan_ctx && m_vulkan_ctx->get_vulkan_device()) {
    (void)m_vulkan_ctx->get_vulkan_device()->waitIdle();
  }

  // Clear all layers FIRST (before destroying windows/views)
  // This ensures OnDetach is called while resources are still valid
  global_layers.clear();
  if (view_manager) {
    view_manager->clear();
  }
  scene_manager.clear();

  if (window_manager) {
    window_manager->~WindowManager();
  }
  if (layout_manager) {
    layout_manager->~LayoutManager();
  }
  if (view_manager) {
    view_manager->~ViewManager();
  }
  if (m_imgui_ctx) {
    m_imgui_ctx->shutdown();
    m_imgui_ctx->~SDImGuiContext();
  }
  if (m_renderer) {
    m_renderer->~VulkanRenderer();
  }
  if (m_vulkan_ctx) {
    m_vulkan_ctx->~VulkanContext();
  }
  if (m_glfw_ctx) {
    m_glfw_ctx->~GlfwContext();
  }

  arena_release(engine_arena);
}

void Application::run(const std::atomic<bool>* external_stop) {
  while (is_running) {
    if (external_stop)
      is_running = !external_stop->load(std::memory_order_acquire);
    frame();
  }
}

void Application::frame() {
  timer.begin();
  glfwPollEvents();
  timer.begin_work();

  float dt = timer.get_frame_time();

  for (auto& e : app_event_manager) {
    on_app_event(e);
  }
  app_event_manager.clear();

  while (timer.consume_fixed_step()) {
    for (auto& layer : global_layers)
      layer.on_fixed_update(timer.get_fixed_time_step());
  }

  m_imgui_ctx->begin_frame();
  m_imgui_ctx->begin_dock_space(app_spec.name);

  if (!ImGui::GetCurrentContext())
    return;
  if (ImGui::BeginMainMenuBar()) {
    if (ImGui::BeginMenu("File")) {
      if (ImGui::MenuItem("Exit", "Alt+F4"))
        is_running = false;
      ImGui::EndMenu();
    }
    global_layers.on_imGui_menu_bar();
    for (auto& data : window_manager->get_windows() | std::views::values)
      data.view_layers.on_imGui_menu_bar();
    for (auto& view : view_manager->get_views() | std::views::values)
      view->get_layers().on_imGui_menu_bar();
    ImGui::EndMainMenuBar();
  }

  for (auto& layer : global_layers) {
    layer.on_update(dt);
    layer.on_gui_render();
  }

  window_manager->update_windows(dt);
  view_manager->update_views(dt);

  m_imgui_ctx->end_dock_space();
  m_imgui_ctx->end_frame();

  window_manager->draw_windows(*view_manager);

  timer.end_work();

  m_imgui_ctx->update_platform_windows();
  window_manager->process_pending_closes();
  view_manager->cleanup_closed_views();
}

void Application::on_app_event(EventVariant& e) {
  std::visit(overloaded{[this](AppTerminateEvent&) { is_running = false; }, [](auto&) {}}, e.event);

  global_layers.on_event(e);
}

void Application::dispatch_shader_reload() {
  if (!consume_shader_reload_request())
    return;

  (void)m_vulkan_ctx->get_vulkan_device()->waitIdle();

  for (auto& layer : global_layers)
    layer.on_shader_reload();

  for (auto& view : view_manager->get_views() | std::views::values)
    for (auto& layer : view->get_layers())
      layer.on_shader_reload();

  for (auto& data : window_manager->get_windows() | std::views::values)
    for (auto& layer : data.view_layers)
      layer.on_shader_reload();
}

void Application::clear_game_layers() {
  global_layers.clear();
  if (view_manager) {
    view_manager->clear();
  }
  // Scenes survive reload — the game's on_load reuses them
  // and re-pushes layers/views with the new code.
}

} // namespace sd

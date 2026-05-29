// TODO(docs): Add file-level Doxygen header
//   - @file Application.hpp
//   - @brief Main application class - the central hub of the engine
//   - Architecture overview: Application owns all subsystems
//   - Lifecycle diagram (Init -> Run -> Shutdown)
//   - Hot reload integration notes
#pragma once
#include <atomic>
#include <expected>
#include <format>
#include <functional>
#include <memory>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#include "Core/Base.hpp"
#include "Core/Events/EventManager.hpp"
#include "Core/FrameTimer.hpp"
#include "Core/LayerList.hpp"
#include "Core/Scene.hpp"
#include "Core/View.hpp"
#include "Core/ViewManager.hpp"
#include "Core/WindowManager.hpp"

namespace sd {
class Event;
class GameContext;
class GlfwContext;
class Layer;
class LayoutManager;
class RuntimeStateManager;
class SDImGuiContext;
class VulkanContext;
class VulkanRenderer;
class VulkanWindow;
class Window;
} // namespace SD


namespace sd {

// TODO(docs): Document ApplicationSpecification
//   - Each field's purpose and default value rationale
//   - Example configuration
struct ApplicationSpecification {
  std::string name = "SDEngine App";
  int width = 1600;
  int height = 900;
  bool enableHotReload = true;
  std::string gameSoPath = "libSandboxApp.so";
};

// TODO(docs): Document Application class thoroughly
//   - Class overview and responsibilities
//   - Ownership model (what it owns vs references)
//   - Singleton pattern usage (Get())
//   - Thread safety considerations
//   - Example minimal application setup
class Application {
public:
  explicit Application(const ApplicationSpecification& spec, RuntimeStateManager* state_manager = nullptr);
  virtual ~Application();

  void run(const std::atomic<bool>* external_stop);
  void frame();
  bool is_running() const { return m_is_running; }
  void close() { m_is_running = false; }
  void on_app_event(Event& e);

  // Layer management
  template<typename T, typename... Args>
    requires std::is_base_of_v<Layer, T>
  T& push_global_layer(Args&&... args) {
    return m_global_layers.push_layer<T>(std::forward<Args>(args)...);
  }

  // Window management
  WindowId create_window(const WindowProps& props) { return m_window_manager->create(props); }
  Window& get_window(WindowId id) { return m_window_manager->get_window(id); }
  VulkanWindow& get_render_window(WindowId id) { return m_window_manager->get_render_window(id); }

  template<typename T, typename... Args>
    requires std::is_base_of_v<Layer, T>
  T& push_view_layer(WindowId id, Args&&... args) {
    auto& windows = m_window_manager->get_windows();
    if (!windows.contains(id)) {
      engine_abort(std::format("Attempted to push layer to invalid window ID: {}",
                        static_cast<uint32_t>(id)));
    }
    return windows[id].view_layers.push_layer<T>(std::forward<Args>(args)...);
  }

  // View management
  template<typename T, typename... Args>
    requires std::is_base_of_v<View, T>
  T& create_view(std::string name, Args&&... args) {
    return m_view_manager->create<T>(std::move(name), std::forward<Args>(args)...);
  }

  using ViewResult = ViewManager::ViewResult;

  ViewResult get_view(ViewId id) { return m_view_manager->get(id); }
  ViewResult get_view(const std::string& name) { return m_view_manager->get(name); }
  std::expected<ViewId, ViewError> get_view_id(const std::string& name) const {
    return m_view_manager->GetId(name);
  }

  ViewError remove_view(ViewId id) { return m_view_manager->remove(id); }
  ViewError remove_view(const std::string& name) { return m_view_manager->remove(name); }

  template<typename T, typename... Args>
    requires std::is_base_of_v<Layer, T>
  std::expected<std::reference_wrapper<T>, ViewError> push_layer_to_view(ViewId id, Args&&... args) {
    return m_view_manager->push_layer<T>(id, std::forward<Args>(args)...);
  }

  template<typename T, typename... Args>
    requires std::is_base_of_v<Layer, T>
  std::expected<std::reference_wrapper<T>, ViewError> push_layer_to_view(const std::string& name,
                                                                      Args&&... args) {
    auto id_res = m_view_manager->GetId(name);
    if (!id_res)
      return std::unexpected(id_res.error());
    return m_view_manager->push_layer<T>(*id_res, std::forward<Args>(args)...);
  }

  const std::unordered_map<ViewId, std::unique_ptr<View>>& get_views() const {
    return m_view_manager->get_views();
  }

  // Global layer access (needed for layout management)
  LayerList& get_global_layers() { return m_global_layers; }
  const LayerList& get_global_layers() const { return m_global_layers; }

  // Scene management
  Scene* create_scene(const std::string& name);
  Scene* get_scene(const std::string& name) const;
  std::vector<Scene*> get_scenes();

  // Game Context (for hot reload)
  void set_game_context(GameContext* ctx);
  void set_game_handle(void* handle) { m_game_handle = handle; }
  GameContext* get_game_context() const { return m_game_context; }
  void clear_game_layers();
  void reload_game();

  // Hot Reload
  void set_hot_reload_enabled(bool enabled) { m_hot_reload_enabled = enabled; }
  bool is_hot_reload_enabled() const { return m_hot_reload_enabled; }
  void reload_shaders() const;

  // Accessors
  VulkanContext& get_vulkan_context() { return *m_vulkan_ctx; }
  VulkanRenderer& get_renderer() { return *m_renderer; }
  SDImGuiContext& get_im_gui_context() { return *m_im_gui_ctx; }
  WindowManager& get_window_manager() { return *m_window_manager; }
  ViewManager& get_view_manager() { return *m_view_manager; }
  LayoutManager& get_layout_manager() { return *m_layout_manager; }

  static Application& get() {
    assert(s_instance && "Application::Get() called before Application was constructed");
    return *s_instance;
  }
  float get_frame_work_time() const { return m_timer.get_frame_work_time(); }
  void add_gpu_wait_time(float t) { m_timer.add_gpu_wait_time(t); }
  FrameTimer& get_frame_timer() { return m_timer; }

private:
  static Application* s_instance;
  ApplicationSpecification m_spec;
  bool m_is_running = true;
  bool m_hot_reload_enabled = true;
  float m_hot_reload_timer = 0.0f;

  std::unique_ptr<GlfwContext> m_glfw_ctx;
  std::unique_ptr<VulkanContext> m_vulkan_ctx;
  std::unique_ptr<VulkanRenderer> m_renderer;
  std::unique_ptr<SDImGuiContext> m_im_gui_ctx;

  std::unique_ptr<WindowManager> m_window_manager;
  std::unique_ptr<ViewManager> m_view_manager;
  std::unique_ptr<LayoutManager> m_layout_manager;

  LayerList m_global_layers;
  EventManager m_app_event_manager;

  std::vector<std::unique_ptr<Scene>> m_scenes;
  GameContext* m_game_context = nullptr;
  void* m_game_handle = nullptr;

  FrameTimer m_timer;
  RuntimeStateManager* m_state_manager;
};

} // namespace SD

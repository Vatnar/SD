// TODO(docs): Add file-level Doxygen header
//   - @file Application.hpp
//   - @brief Main application class - the central hub of the engine
//   - Architecture overview: Application owns all subsystems
//   - Lifecycle diagram (Init -> Run -> Shutdown)
//   - Hot reload integration notes
#pragma once
#include <atomic>
#include <expected>
#include <functional>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>

#include "SD/core/ApplicationRuntime.hpp"
#include "SD/core/EngineServices.hpp"
#include "SD/core/FrameTimer.hpp"
#include "SD/core/LayerList.hpp"
#include "SD/core/Scene.hpp"
#include "SD/core/SceneManager.hpp"
#include "SD/core/View.hpp"
#include "SD/core/ViewManager.hpp"
#include "SD/core/WindowManager.hpp"
#include "SD/core/events/EventManager.hpp"
#include "SD/export.hpp"

namespace sd {
class Event;
class Layer;
class LayoutManager;
class RuntimeStateManager;
class VulkanWindow;
class Window;
} // namespace sd


namespace sd {

// TODO(docs): Document ApplicationSpecification
//   - Each field's purpose and default value rationale
//   - Example configuration
struct ApplicationSpecification {
  std::string name            = "SDEngine App";
  int         width           = 1600;
  int         height          = 900;
  bool        enableHotReload = true;
  std::string gameSoPath      = "libSandboxApp.so";
};

// TODO(docs): Document Application class thoroughly
//   - Class overview and responsibilities
//   - Ownership model (what it owns vs references)
//   - Singleton pattern usage (Get())
//   - Thread safety considerations
//   - Example minimal application setup
class SD_EXPORT Application {
public:
  explicit Application(const ApplicationSpecification& spec,
                       RuntimeStateManager*            state_manager = nullptr);
  virtual ~Application();

  Application(const Application&)            = delete;
  Application& operator=(const Application&) = delete;
  Application(Application&&)                 = delete;
  Application& operator=(Application&&)      = delete;


  using ViewResult = ViewManager::ViewResult;

  void run(const std::atomic<bool>* external_stop);

  void frame();

  void close() { is_running = false; }

  void on_app_event(Event& e);

  // Layer management
  template<typename T, typename... Args>
    requires std::is_base_of_v<Layer, T>
  T& push_layer(Args&&... args) {
    auto& layer = global_layers.push_layer<T>(std::forward<Args>(args)...);
    layer.m_app = this;
    return layer;
  }

  // Window management
  [[nodiscard]] WindowId create_window(const WindowProps& props) {
    return window_manager->create(props);
  }
  [[nodiscard]] Window& get_window(const WindowId id) const {
    return window_manager->get_window(id);
  }
  [[nodiscard]] VulkanWindow& get_render_window(const WindowId id) const {
    return window_manager->get_render_window(id);
  }

  template<typename T, typename... Args>
    requires std::is_base_of_v<Layer, T>
  T& push_window_layer(WindowId id, Args&&... args);

  template<typename T, typename... Args>
    requires std::is_base_of_v<View, T>
  T& create_view(std::string name, Args&&... args) {
    auto& view = view_manager->create<T>(std::move(name), std::forward<Args>(args)...);
    view.m_app = this;
    return view;
  }


  [[nodiscard]] ViewResult get_view(const ViewId id) const { return view_manager->get(id); }
  [[nodiscard]] ViewResult get_view(const std::string& name) const {
    return view_manager->get(name);
  }
  [[nodiscard]] std::expected<ViewId, ViewError> get_view_id(const std::string& name) const {
    return view_manager->get_id(name);
  }

  [[nodiscard]] ViewError remove_view(ViewId id) { return view_manager->remove(id); }
  [[nodiscard]] ViewError remove_view(const std::string& name) {
    return view_manager->remove(name);
  }

  template<typename T, typename... Args>
    requires std::is_base_of_v<Layer, T>
  std::expected<std::reference_wrapper<T>, ViewError> push_layer_to_view(ViewId id, Args&&... args);

  template<typename T, typename... Args>
    requires std::is_base_of_v<Layer, T>
  std::expected<std::reference_wrapper<T>, ViewError> push_layer_to_view(const std::string& name,
                                                                         Args&&... args);

  // Scene management
  Scene*               create_scene(const std::string& name) { return scene_manager.create(name); }
  [[nodiscard]] Scene* get_scene(const std::string& name) const { return scene_manager.get(name); }

  void clear_game_layers();

  void request_restart() { m_restart_requested.store(true, std::memory_order_release); }
  [[nodiscard]] bool consume_restart_request() {
    return m_restart_requested.exchange(false, std::memory_order_acq_rel);
  }

  void request_shader_reload() { m_shader_reload_requested.store(true, std::memory_order_release); }
  [[nodiscard]] bool consume_shader_reload_request() {
    return m_shader_reload_requested.exchange(false, std::memory_order_acq_rel);
  }

  [[nodiscard]] EngineServices services() const {
    return EngineServices{
        .glfw     = *m_glfw_ctx,
        .vulkan   = *m_vulkan_ctx,
        .renderer = *m_renderer,
        .imgui    = *m_imgui_ctx,
    };
  }

  [[nodiscard]] ApplicationRuntime runtime() {
    return ApplicationRuntime{
        .views              = *view_manager,
        .scenes             = scene_manager,
        .layout             = *layout_manager,
        .events             = app_event_manager,
        .timer              = timer,
        .global_layers      = global_layers,
        .hot_reload_enabled = hot_reload_enabled,
    };
  }

public:
  bool is_running              = true;
  bool hot_reload_enabled      = true;
  bool game_code_reload_paused = false;


  ApplicationSpecification app_spec;


  // todo: abstract


  std::unique_ptr<WindowManager> window_manager;
  std::unique_ptr<ViewManager>   view_manager;
  std::unique_ptr<LayoutManager> layout_manager;
  SceneManager                   scene_manager;

  LayerList    global_layers;
  EventManager app_event_manager;

  RuntimeStateManager* state_manager;
  FrameTimer           timer;

private:
  std::atomic<bool> m_restart_requested{false};
  std::atomic<bool> m_shader_reload_requested{false};

  std::unique_ptr<GlfwContext>    m_glfw_ctx;
  std::unique_ptr<VulkanContext>  m_vulkan_ctx;
  std::unique_ptr<VulkanRenderer> m_renderer;
  std::unique_ptr<SDImGuiContext> m_imgui_ctx;
};


} // namespace sd


#include "impl/Application.inl"

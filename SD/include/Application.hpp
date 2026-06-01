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
#include <unordered_map>
#include <utility>
#include <vector>

#include "core/ApplicationRuntime.hpp"
#include "core/EngineServices.hpp"
#include "core/FrameTimer.hpp"
#include "core/LayerList.hpp"
#include "core/SceneManager.hpp"
#include "core/Scene.hpp"
#include "core/View.hpp"
#include "core/ViewManager.hpp"
#include "core/WindowManager.hpp"
#include "core/events/EventManager.hpp"

namespace sd {
class Event;
class GameContext;
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
class Application {
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
  T& push_global_layer(Args&&... args) {
    return global_layers.push_layer<T>(std::forward<Args>(args)...);
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
  T& push_view_layer(WindowId id, Args&&... args);

  template<typename T, typename... Args>
    requires std::is_base_of_v<View, T>
  T& create_view(std::string name, Args&&... args) {
    return view_manager->create<T>(std::move(name), std::forward<Args>(args)...);
  }


  [[nodiscard]] ViewResult get_view(const ViewId id) const { return view_manager->get(id); }
  [[nodiscard]] ViewResult get_view(const std::string& name) const {
    return view_manager->get(name);
  }
  [[nodiscard]] std::expected<ViewId, ViewError> get_view_id(const std::string& name) const {
    return view_manager->GetId(name);
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
  void reload_game();

  void reload_shaders() const;

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
  bool  is_running         = true;
  bool  hot_reload_enabled = true;
  float hot_reload_timer   = 0.0f;


  ApplicationSpecification app_spec;


  // todo: abstract


  std::unique_ptr<WindowManager> window_manager;
  std::unique_ptr<ViewManager>   view_manager;
  std::unique_ptr<LayoutManager> layout_manager;
  SceneManager                    scene_manager;

  LayerList    global_layers;
  EventManager app_event_manager;
  GameContext*                        game_context = nullptr;

  RuntimeStateManager* state_manager;
  FrameTimer           timer;

private:
  std::unique_ptr<GlfwContext>    m_glfw_ctx;
  std::unique_ptr<VulkanContext>  m_vulkan_ctx;
  std::unique_ptr<VulkanRenderer> m_renderer;
  std::unique_ptr<SDImGuiContext> m_imgui_ctx;

  void* m_game_handle = nullptr;
};


} // namespace sd


#include "impl/Application.inl"

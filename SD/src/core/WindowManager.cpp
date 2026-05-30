#include "core/WindowManager.hpp"

#include "Application.hpp"
#include "core/events/window/window_events.hpp"
#include "core/SDImGuiContext.hpp"
#include "core/ViewManager.hpp"
#include "core/vulkan/VulkanRenderer.hpp"

namespace sd {

WindowManager::WindowManager() = default;
WindowManager::~WindowManager() = default;

WindowId WindowManager::create(const WindowProps& props) {
  assert(!props.title.empty() && "Window title must not be empty");
  assert(props.width > 0 && props.height > 0 && "Window dimensions must be positive");

  WindowId id = m_next_window_id++;

  auto window =
      WindowBuilder().set_title(props.title.c_str()).set_size(props.width, props.height).build();

  assert(window && "Window must be created");

  auto& app = Application::get();
  auto& vulkan_ctx = app.get_vulkan_context();

  if (!vulkan_ctx.is_initialized()) {
    vulkan_ctx.init(*window);
  }

  auto vw = std::make_unique<VulkanWindow>(*window, vulkan_ctx);

  WindowData data;
  data.logic = std::move(window);
  data.render = std::move(vw);

  m_windows[id] = std::move(data);
  return id;
}

void WindowManager::destroy(WindowId id) {
  auto it = m_windows.find(id);
  assert(it != m_windows.end() && "Cannot destroy window,  window ID does not exist");

  auto& app = Application::get();
  (void)app.get_vulkan_context().get_vulkan_device()->waitIdle();
  m_windows.erase(id);
}

void WindowManager::process_pending_closes() {
  for (WindowId id : m_pending_close) {
    destroy(id);
  }
  m_pending_close.clear();
}

Window& WindowManager::get_window(WindowId id) {
  auto it = m_windows.find(id);
  assert(it != m_windows.end() && "Window ID does not exist");
  return *it->second.logic;
}

VulkanWindow& WindowManager::get_render_window(WindowId id) {
  auto it = m_windows.find(id);
  assert(it != m_windows.end() &&  "Window ID does not exist");
  assert(it->second.render && "Render window must be valid");
  return *it->second.render;
}

void WindowManager::update_windows(float dt) {
  for (auto& [id, data] : m_windows) {
    update_window(id, data, dt);
  }
}

void WindowManager::draw_windows(ViewManager& viewManager) {
  assert(!m_windows.empty() && "Must have at least one window to draw");

  for (auto& [id, data] : m_windows) {
    draw_window(id, data, viewManager);
  }
}

void WindowManager::update_window(WindowId id, WindowData& data, float dt) {
  assert(data.logic &&  "Logic window must be valid");
  assert(data.render &&  "Render window must be valid");

  Window& window = *data.logic;
  VulkanWindow& vw = *data.render;
  LayerList& layers = data.view_layers;
  auto& app = Application::get();

  if (vw.is_minimized())
    return;

  for (auto& e : window.get_event_manager()) {
    EventDispatcher dispatcher(*e);

    dispatcher.dispatch<WindowResizeEvent>([&](const WindowResizeEvent& event) {
      vw.resize(event.width, event.height);
      return false;
    });

    dispatcher.dispatch<WindowCloseEvent>([&](const WindowCloseEvent&) {
      if (id == WindowId{}) {
        app.close();
      } else {
        m_pending_close.push_back(id);
      }
      return true;
    });

    layers.on_event(*e);
    // Note: Application-level global layer and view event dispatching
    // should probably happen in Application::Run or some other unified way.
    // For now, we'll let Application handle its own event distribution.
    app.on_app_event(*e);
  }
  window.get_event_manager().clear();

  for (auto& layer : layers)
    layer->on_update(dt);
  for (auto& layer : layers)
    layer->on_gui_render();
}

void WindowManager::draw_window(WindowId id, WindowData& data, ViewManager& viewManager) {
  assert(data.render&& "Render window must be valid");

  VulkanWindow& vw = *data.render;
  LayerList& layers = data.view_layers;
  auto& app = Application::get();
  auto& renderer = app.get_renderer();

  if (vw.is_minimized())
    return;

  if (vw.is_framebuffer_resized()) {
    vw.recreate_swapchain(layers);
    vw.reset_framebuffer_resized();
  }

  // Acquire Image
  auto& frame_sync = vw.get_frame_sync();
  auto acquire_res = vw.get_vulkan_images(frame_sync.image_acquired);

  if (!acquire_res) {
    if (acquire_res.error() == vk::Result::eErrorOutOfDateKHR) {
      vw.recreate_swapchain(layers);
    }
    return;
  }
  vk::CommandBuffer cmd = renderer.begin_command_buffer(vw);

  // 1. Offscreen viewports (Handled by ViewManager)
  viewManager.render_views(cmd);

  // 2. Begin Main Render Pass
  renderer.begin_render_pass(vw);

  // 3. Render Layers
  for (auto& layer : layers)
    layer->on_render(cmd);

  if (id == WindowId{}) {
    app.get_im_gui_context().render_draw_data(cmd);
  }

  vk::Result present_res = renderer.end_frame(vw);
  if (present_res == vk::Result::eErrorOutOfDateKHR || present_res == vk::Result::eSuboptimalKHR) {
    vw.recreate_swapchain(layers);
  }
}

} // namespace SD

#include "SD/core/WindowManager.hpp"

#include <utility>

#include "SD/core/SDImGuiContext.hpp"
#include "SD/core/ViewManager.hpp"
#include "SD/core/events/window/window_events.hpp"
#include "SD/core/vulkan/VulkanRenderer.hpp"

namespace sd {

WindowManager::WindowManager(const EngineServices&         services,
                             const WindowManagerCallbacks& callbacks,
                             Arena*                        arena) :
  window_arena(arena), m_vulkan_ctx(services.vulkan), m_imgui_ctx(services.imgui),
  m_renderer(services.renderer), m_callbacks(callbacks) {
}

WindowManager::~WindowManager() {
  for (auto& [id, data] : m_windows) {
    data.render->~VulkanWindow();
    data.logic->~Window();
  }
}

WindowId WindowManager::create(const WindowProps& props, Arena* arena) {
  ASSERT(!props.title.empty() && "Window title must not be empty");
  ASSERT(props.width > 0 && props.height > 0 && "Window dimensions must be positive");

  WindowId id = m_next_window_id++;

  Window* window = WindowBuilder()
                       .set_title(props.title.c_str())
                       .set_size(props.width, props.height)
                       .build(arena);

  ASSERT(window && "Window must be created");


  if (!m_vulkan_ctx.is_initialized()) {
    m_vulkan_ctx.init(*window);
  }

  auto* vw = arena_push<VulkanWindow>(arena);
  new (vw) VulkanWindow(*window, m_vulkan_ctx);

  WindowData data;
  data.logic  = window;
  data.render = vw;

  m_windows.emplace(id, std::move(data));
  return id;
}

void WindowManager::destroy(WindowId id) {
  auto it = m_windows.find(id);
  ASSERT(it != m_windows.end() && "Cannot destroy window,  window ID does not exist");

  (void)m_vulkan_ctx.get_vulkan_device()->waitIdle();
  it->second.render->~VulkanWindow();
  it->second.logic->~Window();
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
  ASSERT(it != m_windows.end() && "Window ID does not exist");
  return *it->second.logic;
}

VulkanWindow& WindowManager::get_render_window(WindowId id) {
  auto it = m_windows.find(id);
  ASSERT(it != m_windows.end() && "Window ID does not exist");
  ASSERT(it->second.render && "Render window must be valid");
  return *it->second.render;
}

void WindowManager::update_windows(float dt) {
  for (auto& [id, data] : m_windows) {
    update_window(id, data, dt);
  }
}

void WindowManager::draw_windows(ViewManager& viewManager) {
  ASSERT(!m_windows.empty() && "Must have at least one window to draw");

  for (auto& [id, data] : m_windows) {
    draw_window(id, data, viewManager);
  }
}

void WindowManager::update_window(WindowId id, WindowData& data, float dt) {
  ASSERT(data.logic && "Logic window must be valid");
  ASSERT(data.render && "Render window must be valid");

  Window&       window = *data.logic;
  VulkanWindow& vw     = *data.render;
  LayerList&    layers = data.view_layers;

  if (vw.is_minimized())
    return;

  for (auto& e : window.get_event_manager()) {
    std::visit(overloaded{[&](WindowResizeEvent& event) { vw.resize(event.width, event.height); },
                          [&](WindowCloseEvent&) {
                            if (id == WindowId{}) {
                              m_callbacks.close_app();
                            } else {
                              m_pending_close.push_back(id);
                            }
                            e.handled = true;
                          },
                          [](auto&) {}},
               e.event);

    layers.on_event(e);
    m_callbacks.on_app_event(e);
  }
  window.get_event_manager().clear();

  layers.update(dt);
  layers.gui_render();
}

void WindowManager::draw_window(WindowId id, WindowData& data, ViewManager& viewManager) {
  ASSERT(data.render && "Render window must be valid");

  VulkanWindow& vw     = *data.render;
  LayerList&    layers = data.view_layers;

  if (vw.is_minimized())
    return;

  if (vw.is_framebuffer_resized()) {
    vw.recreate_swapchain(layers);
    vw.reset_framebuffer_resized();
  }

  auto& device     = m_vulkan_ctx.get_vulkan_device();
  auto& frame_sync = vw.get_frame_sync();
  (void)device->waitForFences(1, &*frame_sync.in_flight, true, UINT64_MAX);

  auto acquire_res = vw.get_vulkan_images(frame_sync.image_acquired);

  if (!acquire_res) {
    if (acquire_res.error() == vk::Result::eErrorOutOfDateKHR) {
      vw.recreate_swapchain(layers);
    }
    return;
  }
  vk::CommandBuffer cmd = m_renderer.begin_command_buffer(vw);

  viewManager.render_views(cmd);

  m_renderer.begin_render_pass(vw);

  layers.on_render(cmd);

  if (id == WindowId{}) {
    m_imgui_ctx.render_draw_data(cmd);
  }

  vk::Result present_res = m_renderer.end_frame(vw);
  if (present_res == vk::Result::eErrorOutOfDateKHR || present_res == vk::Result::eSuboptimalKHR) {
    vw.recreate_swapchain(layers);
  }
}

} // namespace sd

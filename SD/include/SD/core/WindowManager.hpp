// TODO(docs): Add file-level Doxygen header
//   - @file WindowManager.hpp
//   - @brief Multi-window management for GLFW and Vulkan windows
//   - Relationship to ViewManager and Application
#pragma once

#include <unordered_map>
#include <vector>

#include "SD/arena.hpp"
#include "SD/core/EngineServices.hpp"
#include "SD/core/LayerList.hpp"
#include "SD/core/Window.hpp"
#include "SD/core/id_types.hpp"
#include "SD/core/vulkan/VulkanWindow.hpp"
#include "SDImGuiContext.hpp"
#include "vulkan/VulkanRenderer.hpp"

namespace sd {

struct ViewManager;

// TODO(docs): Document WindowProps struct
//   - Each field's purpose
//   - Default values rationale
struct WindowProps {
  std::string title;
  int         width, height;
  explicit WindowProps(const std::string& title = "SD Engine", int width = 1280, int height = 720) :
    title(title), width(width), height(height) {}
};

struct WindowManagerCallbacks {
  std::function<void()>              close_app;
  std::function<void(EventVariant&)> on_app_event;
};
// TODO(docs): Document WindowManager class
//   - Purpose: Manages multiple windows (GLFW + Vulkan)
//   - Window lifecycle (Create, Destroy, ProcessPendingCloses)
//   - WindowData structure (logic window, render window, layers)
//   - Update/Draw loop integration
//   - Example: Creating and managing multiple windows
struct WindowManager {
  WindowManager(const EngineServices&         services,
                const WindowManagerCallbacks& callbacks,
                Arena*                        arena);
  ~WindowManager();

  WindowId create(const WindowProps& props, Arena* arena);
  void     destroy(WindowId id);
  void     process_pending_closes();

  Window&       get_window(WindowId id);
  VulkanWindow& get_render_window(WindowId id);

  struct WindowData {
    Window*       logic;
    VulkanWindow* render;
    LayerList     view_layers;
  };

  auto&       get_windows() { return m_windows; }
  const auto& get_windows() const { return m_windows; }

  void update_windows(float dt);
  void draw_windows(ViewManager& viewManager);

  Arena* window_arena;


  void update_window(WindowId id, WindowData& data, float dt);
  void draw_window(WindowId id, WindowData& data, ViewManager& viewManager);

  VulkanContext&  m_vulkan_ctx;
  SDImGuiContext& m_imgui_ctx;
  VulkanRenderer& m_renderer;

  WindowManagerCallbacks m_callbacks;

  std::unordered_map<WindowId, WindowData> m_windows;
  WindowId                                 m_next_window_id;
  std::vector<WindowId>                    m_pending_close;
};

} // namespace sd

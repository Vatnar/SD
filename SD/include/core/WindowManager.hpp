// TODO(docs): Add file-level Doxygen header
//   - @file WindowManager.hpp
//   - @brief Multi-window management for GLFW and Vulkan windows
//   - Relationship to ViewManager and Application
#pragma once

#include "core/LayerList.hpp"
#include "core/Window.hpp"
#include "core/id_types.hpp"
#include "core/vulkan/VulkanWindow.hpp"

#include <memory>
#include <unordered_map>
#include <vector>

namespace sd {

class ViewManager;

// TODO(docs): Document WindowProps struct
//   - Each field's purpose
//   - Default values rationale
struct WindowProps {
  std::string title;
  int width, height;
  explicit WindowProps(const std::string& title = "SD Engine", int width = 1280, int height = 720) :
    title(title), width(width), height(height) {}
};

// TODO(docs): Document WindowManager class
//   - Purpose: Manages multiple windows (GLFW + Vulkan)
//   - Window lifecycle (Create, Destroy, ProcessPendingCloses)
//   - WindowData structure (logic window, render window, layers)
//   - Update/Draw loop integration
//   - Example: Creating and managing multiple windows
class WindowManager {
public:
  WindowManager();
  ~WindowManager();

  WindowId create(const WindowProps& props);
  void destroy(WindowId id);
  void process_pending_closes();

  Window& get_window(WindowId id);
  VulkanWindow& get_render_window(WindowId id);

  struct WindowData {
    std::unique_ptr<Window> logic;
    std::unique_ptr<VulkanWindow> render;
    LayerList view_layers;

    WindowData() = default;
    WindowData(WindowData&&) = default;
    WindowData& operator=(WindowData&&) = default;
    WindowData(const WindowData&) = delete;
    WindowData& operator=(const WindowData&) = delete;
  };

  auto& get_windows() { return m_windows; }
  const auto& get_windows() const { return m_windows; }

  void update_windows(float dt);
  void draw_windows(ViewManager& viewManager);

private:
  void update_window(WindowId id, WindowData& data, float dt);
  void draw_window(WindowId id, WindowData& data, ViewManager& viewManager);

  std::unordered_map<WindowId, WindowData> m_windows;
  WindowId m_next_window_id;
  std::vector<WindowId> m_pending_close;
};

} // namespace SD

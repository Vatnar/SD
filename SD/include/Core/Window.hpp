// TODO(docs): Add file-level Doxygen header
//   - @file Window.hpp
//   - @brief Window abstraction over GLFW
//   - Event handling through callbacks
//   - Builder pattern for window creation
#pragma once
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <functional>
#include <memory>

#include "Events/EventManager.hpp"
#include "LayerList.hpp"
#include "Vulkan/VulkanConfig.hpp"

namespace sd {
// TODO(docs): Document callback type aliases
//   - Each callback's signature and when it's called
using ResizeCallbackFn = std::function<void(int, int)>;
using KeyCallbackFn = std::function<void(int, int, int, int)>;
using ScrollCallbackFn = std::function<void(double, double)>;
using CursorCallbackFn = std::function<void(double, double)>;
using MouseButtonCallbackFn = std::function<void(int, int, int)>;
using RefreshCallbackFn = std::function<void()>;
using CharCallbackFn = std::function<void(unsigned int)>;


class Window;

// TODO(docs): Document WindowDesc struct
//   - Each field's purpose
//   - Which are required vs optional
struct WindowDesc {
  const char* title{};
  int width{};
  int height{};

  ResizeCallbackFn resize_callback{nullptr};
  KeyCallbackFn key_callback{nullptr};
  ScrollCallbackFn scroll_callback{nullptr};
  CursorCallbackFn cursor_callback{nullptr};
  MouseButtonCallbackFn mouse_button_callback{nullptr};
  RefreshCallbackFn refresh_callback{nullptr};
  CharCallbackFn char_callback{nullptr};
};


class Window {
public:
  Window(int width, int height, const std::string& title);
  explicit Window(const WindowDesc& desc);


  Window(Window&&) = delete;
  Window& operator=(Window&&) = delete;

  Window(const Window&) = delete;
  Window& operator=(const Window&) = delete;
  /**
   * @brief ONLY CALL if ALL windows are minimized
   */
  static void wait_events() { glfwWaitEvents(); }


  ~Window();
  void set_resize_callback(const ResizeCallbackFn& callback) { m_resize_callback = callback; }
  void set_key_callback(const KeyCallbackFn& callback) { m_key_callback = callback; }
  void set_scroll_callback(const ScrollCallbackFn& callback) { m_scroll_callback = callback; }
  void set_cursor_callback(const CursorCallbackFn& callback) { m_cursor_callback = callback; }
  void set_mouse_button_callback(const MouseButtonCallbackFn& callback) {
    m_mouse_button_callback = callback;
  }
  void set_refresh_callback(const RefreshCallbackFn& callback) { m_refresh_callback = callback; }
  void set_char_callback(const CharCallbackFn& callback) { m_char_callback = callback; }

  [[nodiscard]] GLFWwindow* get_native_handle() const { return m_handle; }
  [[nodiscard]] std::pair<int, int> get_window_size() const;
  [[nodiscard]] std::pair<int, int> get_framebuffer_size() const;
  [[nodiscard]] bool should_close() const { return glfwWindowShouldClose(m_handle); }
  vk::UniqueSurfaceKHR create_window_surface(vk::UniqueInstance& instance,
                                           const VkAllocationCallbacks* allocation_callback) const;
  EventManager& get_event_manager() { return m_window_event_manager; }

  LayerList layer_stack;

private:
  GLFWwindow* m_handle;


  ResizeCallbackFn m_resize_callback;
  KeyCallbackFn m_key_callback;
  ScrollCallbackFn m_scroll_callback;
  CursorCallbackFn m_cursor_callback;
  MouseButtonCallbackFn m_mouse_button_callback;
  RefreshCallbackFn m_refresh_callback;
  CharCallbackFn m_char_callback;

  EventManager m_window_event_manager;


  static void dispatch_resize(GLFWwindow* window, int width, int height);
  static void dispatch_close(GLFWwindow* window);
  static void dispatch_key(GLFWwindow* window, int key, int scancode, int action, int mods);
  static void dispatch_scroll(GLFWwindow* window, double xOffset, double yOffset);
  static void dispatch_cursor(GLFWwindow* window, double xPos, double yPos);
  static void dispatch_mouse_button(GLFWwindow* window, int button, int action, int mods);
  static void dispatch_refresh(GLFWwindow* window);
  static void dispatch_char_dispatch_char(GLFWwindow* window, unsigned int keycode);
};

class WindowBuilder {
public:
  WindowBuilder() : m_desc({}) {}
  WindowBuilder& set_title(const char* title);
  WindowBuilder& set_size(int width, int height);
  WindowBuilder& set_width(int width);
  WindowBuilder& set_height(int height);

  WindowBuilder& set_resize_callback(const ResizeCallbackFn& callback);
  WindowBuilder& set_key_callback(const KeyCallbackFn& callback);
  WindowBuilder& set_scroll_callback(const ScrollCallbackFn& callback);
  WindowBuilder& set_cursor_callback(const CursorCallbackFn& callback);
  WindowBuilder& set_mouse_button_callback(const MouseButtonCallbackFn& callback);
  WindowBuilder& set_refresh_callback_set_refresh_callback(const RefreshCallbackFn& callback);
  WindowBuilder& set_char_callback(const CharCallbackFn& callback);

  [[nodiscard]] std::unique_ptr<Window> build() const;

private:
  WindowDesc m_desc;
};
} // namespace SD

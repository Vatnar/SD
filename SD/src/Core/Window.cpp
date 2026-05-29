#include "Core/Window.hpp"

#include "Core/Events/window/KeyboardEvents.hpp"
#include "Core/Events/window/MouseEvents.hpp"
#include "Core/Events/window/WindowEvents.hpp"
#include "Core/Logging.hpp"
#include "Utils/Utils.hpp"


sd::Window::Window(int width, int height, const std::string& title) {
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

  m_handle = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);
  if (!m_handle) {
    const char* error = nullptr;
    glfwGetError(&error);
    log::engine::error("Failed to create GLFW window with error: {}", error ? error : "unknown");
    engine_abort("Failed to create GLFW window");
  }

  glfwSetWindowUserPointer(m_handle, this);

  // TODO: We have a bunch of callbacks, maybe we use a builder pattern so we can have some default
  // callbacks and stuff yk.
  glfwSetWindowSizeCallback(m_handle, dispatch_resize);
  glfwSetFramebufferSizeCallback(m_handle, dispatch_resize);
  glfwSetKeyCallback(m_handle, dispatch_key);
  glfwSetScrollCallback(m_handle, dispatch_scroll);
  glfwSetCursorPosCallback(m_handle, dispatch_cursor);
  glfwSetMouseButtonCallback(m_handle, dispatch_mouse_button);
  glfwSetWindowCloseCallback(m_handle, dispatch_close);
  glfwSetWindowRefreshCallback(m_handle, dispatch_refresh);
  glfwSetCharCallback(m_handle, dispatch_char_dispatch_char);
}


sd::Window::Window(const WindowDesc& desc) {
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

  m_handle = glfwCreateWindow(desc.width, desc.height, desc.title, nullptr, nullptr);
  if (!m_handle) {
    const char* error = nullptr;
    glfwGetError(&error);
    log::engine::error("Failed to create GLFW window with error: {}", error ? error : "unknown");
    engine_abort("Failed to create GLFW window");
  }
  glfwSetWindowUserPointer(m_handle, this);

  ResizeCallbackFn default_resize_callback = [this](int width, int height) {
    m_window_event_manager.push_event<WindowResizeEvent>(width, height);
  };

  KeyCallbackFn default_key_callback = [this](int key, int scancode, int action, int mods) {
    switch (action) {
      case GLFW_PRESS:
        m_window_event_manager.push_event<KeyPressedEvent>(key, scancode, mods, false);
        break;
      case GLFW_REPEAT:
        m_window_event_manager.push_event<KeyPressedEvent>(key, scancode, mods, true);
        break;
      case GLFW_RELEASE:
        m_window_event_manager.push_event<KeyReleasedEvent>(key, scancode, mods);
        break;
      default:
        break;
    }
  };
  ScrollCallbackFn default_scroll_callback = [this](double xOffset, double yOffset) {
    m_window_event_manager.push_event<MouseScrolledEvent>(xOffset, yOffset);
  };
  CursorCallbackFn default_cursor_callback = [this](double xPos, double yPos) {
    m_window_event_manager.push_event<MouseMovedEvent>(xPos, yPos);
  };
  MouseButtonCallbackFn default_mouse_callback = [this](int button, int action, int mods) {
    switch (action) {
      case GLFW_PRESS:
        m_window_event_manager.push_event<MousePressedEvent>(button, mods, false);
        break;
      case GLFW_REPEAT:
        m_window_event_manager.push_event<MousePressedEvent>(button, mods, true);
        break;
      case GLFW_RELEASE:
        m_window_event_manager.push_event<MouseReleasedEvent>(button, mods);
        break;
      default:
        break;
    }
  };
  CharCallbackFn default_char_callback = [this](unsigned int keycode) {
    m_window_event_manager.push_event<KeyTypedEvent>(keycode);
  };

  m_resize_callback = desc.resize_callback ? desc.resize_callback : default_resize_callback;
  m_key_callback = desc.key_callback ? desc.key_callback : default_key_callback;
  m_scroll_callback = desc.scroll_callback ? desc.scroll_callback : default_scroll_callback;
  m_cursor_callback = desc.cursor_callback ? desc.cursor_callback : default_cursor_callback;
  m_mouse_button_callback = desc.mouse_button_callback ? desc.mouse_button_callback : default_mouse_callback;
  m_refresh_callback = desc.refresh_callback ? desc.refresh_callback : []() {
  };
  m_char_callback = desc.char_callback ? desc.char_callback : default_char_callback;

  glfwSetWindowSizeCallback(m_handle, dispatch_resize);
  glfwSetFramebufferSizeCallback(m_handle, dispatch_resize);
  glfwSetKeyCallback(m_handle, dispatch_key);
  glfwSetScrollCallback(m_handle, dispatch_scroll);
  glfwSetCursorPosCallback(m_handle, dispatch_cursor);
  glfwSetMouseButtonCallback(m_handle, dispatch_mouse_button);
  glfwSetWindowCloseCallback(m_handle, dispatch_close);
  glfwSetWindowRefreshCallback(m_handle, dispatch_refresh);
  glfwSetCharCallback(m_handle, dispatch_char_dispatch_char);
}

sd::Window::~Window() {
  if (m_handle)
    glfwDestroyWindow(m_handle);
}

std::pair<int, int> sd::Window::get_window_size() const {
  int w = 0, h = 0;
  glfwGetWindowSize(m_handle, &w, &h);
  return {w, h};
}
std::pair<int, int> sd::Window::get_framebuffer_size() const {
  int w{0}, h{0};
  glfwGetFramebufferSize(m_handle, &w, &h);
  return {w, h};
}


vk::UniqueSurfaceKHR
sd::Window::create_window_surface(vk::UniqueInstance& instance,
                                const VkAllocationCallbacks* allocation_callback) const {
  VkSurfaceKHR surface;
  auto res = glfwCreateWindowSurface(*instance, m_handle, allocation_callback, &surface);
  if (res != VK_SUCCESS) {
    // TODO: Maybe give this info to caller
    engine_abort("Failed to create window surface");
  }
  return vk::UniqueSurfaceKHR{surface, *instance};
}
void sd::Window::dispatch_resize(GLFWwindow* window, int width, int height) {
  if (const auto* self = static_cast<Window*>(glfwGetWindowUserPointer(window));
      self && self->m_resize_callback)
    self->m_resize_callback(width, height);
}
void sd::Window::dispatch_close(GLFWwindow* window) {
  if (auto* self = static_cast<Window*>(glfwGetWindowUserPointer(window))) {
    self->m_window_event_manager.push_event<WindowCloseEvent>();
    glfwSetWindowShouldClose(window, GLFW_FALSE);
  }
}
void sd::Window::dispatch_key(GLFWwindow* window, int key, int scancode, int action, int mods) {
  if (const auto* self = static_cast<Window*>(glfwGetWindowUserPointer(window));
      self && self->m_key_callback)
    self->m_key_callback(key, scancode, action, mods);
}
void sd::Window::dispatch_scroll(GLFWwindow* window, double xOffset, double yOffset) {
  if (const auto* self = static_cast<Window*>(glfwGetWindowUserPointer(window));
      self && self->m_scroll_callback)
    self->m_scroll_callback(xOffset, yOffset);
}
void sd::Window::dispatch_cursor(GLFWwindow* window, double xPos, double yPos) {
  if (const auto* self = static_cast<Window*>(glfwGetWindowUserPointer(window));
      self && self->m_cursor_callback)
    self->m_cursor_callback(xPos, yPos);
}
void sd::Window::dispatch_mouse_button(GLFWwindow* window, int button, int action, int mods) {
  if (const auto* self = static_cast<Window*>(glfwGetWindowUserPointer(window));
      self && self->m_mouse_button_callback)
    self->m_mouse_button_callback(button, action, mods);
}
void sd::Window::dispatch_refresh(GLFWwindow* window) {
  if (const auto* self = static_cast<Window*>(glfwGetWindowUserPointer(window));
      self && self->m_refresh_callback)
    self->m_refresh_callback();
}
void sd::Window::dispatch_char_dispatch_char(GLFWwindow* window, unsigned int keycode) {
  if (const auto* self = static_cast<Window*>(glfwGetWindowUserPointer(window));
      self && self->m_char_callback)
    self->m_char_callback(keycode);
}
sd::WindowBuilder& sd::WindowBuilder::set_title(const char* title) {
  m_desc.title = title;
  return *this;
}
sd::WindowBuilder& sd::WindowBuilder::set_size(const int width, const int height) {
  m_desc.width = width;
  m_desc.height = height;
  return *this;
}
sd::WindowBuilder& sd::WindowBuilder::set_width(const int width) {
  m_desc.width = width;
  return *this;
}
auto sd::WindowBuilder::set_height(const int height) -> sd::WindowBuilder& {
  m_desc.height = height;
  return *this;
}
sd::WindowBuilder& sd::WindowBuilder::set_resize_callback(const ResizeCallbackFn& callback) {
  m_desc.resize_callback = callback;
  return *this;
}
sd::WindowBuilder& sd::WindowBuilder::set_key_callback(const KeyCallbackFn& callback) {
  m_desc.key_callback = callback;
  return *this;
}
sd::WindowBuilder& sd::WindowBuilder::set_scroll_callback(const ScrollCallbackFn& callback) {
  m_desc.scroll_callback = callback;
  return *this;
}
sd::WindowBuilder& sd::WindowBuilder::set_cursor_callback(const CursorCallbackFn& callback) {
  m_desc.cursor_callback = callback;
  return *this;
}
sd::WindowBuilder& sd::WindowBuilder::set_mouse_button_callback(const MouseButtonCallbackFn& callback) {
  m_desc.mouse_button_callback = callback;
  return *this;
}
sd::WindowBuilder& sd::WindowBuilder::set_refresh_callback_set_refresh_callback(const RefreshCallbackFn& callback) {
  m_desc.refresh_callback = callback;
  return *this;
}
sd::WindowBuilder& sd::WindowBuilder::set_char_callback(const CharCallbackFn& callback) {
  m_desc.char_callback = callback;
  return *this;
}
std::unique_ptr<sd::Window> sd::WindowBuilder::build() const {
  return std::make_unique<Window>(m_desc);
}

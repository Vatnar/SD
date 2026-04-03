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

namespace SD {
// TODO(docs): Document callback type aliases
//   - Each callback's signature and when it's called
using ResizeCallback = std::function<void(int, int)>;
using KeyCallback = std::function<void(int, int, int, int)>;
using ScrollCallback = std::function<void(double, double)>;
using CursorCallback = std::function<void(double, double)>;
using MouseButtonCallback = std::function<void(int, int, int)>;
using RefreshCallback = std::function<void()>;
using CharCallback = std::function<void(unsigned int)>;


class Window;

// TODO(docs): Document WindowDesc struct
//   - Each field's purpose
//   - Which are required vs optional
struct WindowDesc {
  const char* title{};
  int width{};
  int height{};

  ResizeCallback resizeCallback{nullptr};
  KeyCallback keyCallback{nullptr};
  ScrollCallback scrollCallback{nullptr};
  CursorCallback cursorCallback{nullptr};
  MouseButtonCallback mouseButtonCallback{nullptr};
  RefreshCallback refreshCallback{nullptr};
  CharCallback charCallback{nullptr};
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
  static void WaitEvents() { glfwWaitEvents(); }


  ~Window();
  void SetResizeCallback(const ResizeCallback& callback) { mResizeCallback = callback; }
  void SetKeyCallback(const KeyCallback& callback) { mKeyCallback = callback; }
  void SetScrollCallback(const ScrollCallback& callback) { mScrollCallback = callback; }
  void SetCursorCallback(const CursorCallback& callback) { mCursorCallback = callback; }
  void SetMouseButtonCallback(const MouseButtonCallback& callback) {
    mMouseButtonCallback = callback;
  }
  void SetRefreshCallback(const RefreshCallback& callback) { mRefreshCallback = callback; }
  void SetCharCallback(const CharCallback& callback) { mCharCallback = callback; }

  [[nodiscard]] GLFWwindow* GetNativeHandle() const { return mHandle; }
  [[nodiscard]] std::pair<int, int> GetWindowSize() const;
  [[nodiscard]] std::pair<int, int> GetFramebufferSize() const;
  [[nodiscard]] bool ShouldClose() const { return glfwWindowShouldClose(mHandle); }
  vk::UniqueSurfaceKHR CreateWindowSurface(vk::UniqueInstance& instance,
                                           const VkAllocationCallbacks* allocationCallback) const;
  EventManager& GetEventManager() { return mWindowEventManager; }

  LayerList LayerStack;

private:
  GLFWwindow* mHandle;


  ResizeCallback mResizeCallback;
  KeyCallback mKeyCallback;
  ScrollCallback mScrollCallback;
  CursorCallback mCursorCallback;
  MouseButtonCallback mMouseButtonCallback;
  RefreshCallback mRefreshCallback;
  CharCallback mCharCallback;

  EventManager mWindowEventManager;


  static void DispatchResize(GLFWwindow* window, int width, int height);
  static void DispatchClose(GLFWwindow* window);
  static void DispatchKey(GLFWwindow* window, int key, int scancode, int action, int mods);
  static void DispatchScroll(GLFWwindow* window, double xOffset, double yOffset);
  static void DispatchCursor(GLFWwindow* window, double xPos, double yPos);
  static void DispatchMouseButton(GLFWwindow* window, int button, int action, int mods);
  static void DispatchRefresh(GLFWwindow* window);
  static void DispatchChar(GLFWwindow* window, unsigned int keycode);
};

class WindowBuilder {
public:
  WindowBuilder() : mDesc({}) {}
  WindowBuilder& SetTitle(const char* title);
  WindowBuilder& SetSize(int width, int height);
  WindowBuilder& SetWidth(int width);
  WindowBuilder& SetHeight(int height);

  WindowBuilder& SetResizeCallback(const ResizeCallback& callback);
  WindowBuilder& SetKeyCallback(const KeyCallback& callback);
  WindowBuilder& SetScrollCallback(const ScrollCallback& callback);
  WindowBuilder& SetCursorCallback(const CursorCallback& callback);
  WindowBuilder& SetMouseButtonCallback(const MouseButtonCallback& callback);
  WindowBuilder& SetRefreshCallback(const RefreshCallback& callback);
  WindowBuilder& SetCharCallback(const CharCallback& callback);

  [[nodiscard]] std::unique_ptr<Window> Build() const;

private:
  WindowDesc mDesc;
};
} // namespace SD

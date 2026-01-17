#pragma once
#include "VulkanConfig.hpp"

#include <GLFW/glfw3.h>
#include <functional>
#include <memory>

#include "EventManager.hpp"
#include "InputEvent.hpp"

using ResizeCallback = std::function<void(int, int)>;
using KeyCallback = std::function<void(int, int, int, int)>;
using ScrollCallback = std::function<void(double, double)>;
using CursorCallback = std::function<void(double, double)>;
using MouseButtonCallback = std::function<void(int, int, int)>;

struct WindowDesc {
  const char* title{};
  int width{};
  int height{};

  ResizeCallback resizeCallback{nullptr};
  KeyCallback keyCallback{nullptr};
  ScrollCallback scrollCallback{nullptr};
  CursorCallback cursorCallback{nullptr};
  MouseButtonCallback mouseButtonCallback{nullptr};
};

class Window {
public:
  Window(int width, int height, const std::string& title);
  explicit Window(const WindowDesc& desc);

  ~Window();
  void SetResizeCallback(const ResizeCallback& callback) { mResizeCallback = callback; }
  void SetKeyCallback(const KeyCallback& callback) { mKeyCallback = callback; }
  void SetScrollCallback(const ScrollCallback& callback) { mScrollCallback = callback; }
  void SetCursorCallback(const CursorCallback& callback) { mCursorCallback = callback; }
  void SetMouseButtonCallback(const MouseButtonCallback& callback) {
    mMouseButtonCallback = callback;
  }

  [[nodiscard]] GLFWwindow* GetNativeHandle() const { return mHandle; }
  [[nodiscard]] std::pair<int, int> GetWindowSize() const;
  [[nodiscard]] bool ShouldClose() const { return glfwWindowShouldClose(mHandle); }
  vk::UniqueSurfaceKHR CreateWindowSurface(vk::UniqueInstance& instance,
                                           const VkAllocationCallbacks* allocationCallback) const;
  std::vector<std::unique_ptr<InputEvent>>& GetEventManager() { return eventManager; }

private:
  GLFWwindow* mHandle;


  ResizeCallback mResizeCallback;
  KeyCallback mKeyCallback;
  ScrollCallback mScrollCallback;
  CursorCallback mCursorCallback;
  MouseButtonCallback mMouseButtonCallback;

  InputEventManager eventManager;


  static void DispatchResize(GLFWwindow* window, int width, int height);
  static void DispatchKey(GLFWwindow* window, int key, int scancode, int action, int mods);
  static void DispatchScroll(GLFWwindow* window, double xOffset, double yOffset);
  static void DispatchCursor(GLFWwindow* window, double xPos, double yPos);
  static void DispatchMouseButton(GLFWwindow* window, int button, int action, int mods);
};

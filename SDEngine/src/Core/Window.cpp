#include "Core/Window.hpp"

#include "Core/Events/Event.hpp"
#include "Core/Events/window/KeyboardEvents.hpp"
#include "Core/Events/window/MouseEvents.hpp"
#include "Core/Events/window/WindowEvents.hpp"
#include "Core/Vulkan/VulkanConfig.hpp"
#include "Utils/Utils.hpp"


namespace SD {
// TODO: Maybe we want a builder pattern instead of window desc.
// TODO: also this repeats a buinch of shit
Window::Window(int width, int height, const std::string& title) {
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

  mHandle = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);
  if (!mHandle) {
    const char* error = nullptr;
    glfwGetError(&error);
    if (const auto logger = spdlog::get("engine"))
      logger->error("Failed to create GLFW window with error: ", error);
    Abort("Failed to create GLFW window");
  }

  glfwSetWindowUserPointer(mHandle, this);

  // TODO: We have a bunch of callbacks, maybe we use a builder pattern so we can have some default
  // callbacks and stuff yk.
  glfwSetWindowSizeCallback(mHandle, DispatchResize);
  glfwSetFramebufferSizeCallback(mHandle, DispatchResize);
  glfwSetKeyCallback(mHandle, DispatchKey);
  glfwSetScrollCallback(mHandle, DispatchScroll);
  glfwSetCursorPosCallback(mHandle, DispatchCursor);
  glfwSetMouseButtonCallback(mHandle, DispatchMouseButton);
  glfwSetWindowCloseCallback(mHandle, DispatchClose);
  glfwSetWindowRefreshCallback(mHandle, DispatchRefresh);
}

Window::Window(const WindowDesc& desc) {
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

  mHandle = glfwCreateWindow(desc.width, desc.height, desc.title, nullptr, nullptr);
  if (!mHandle) {
    const char* error = nullptr;
    glfwGetError(&error);
    if (const auto logger = spdlog::get("engine"))
      logger->error("Failed to create GLFW window with error: ", error);
    Abort("Failed to create GLFW window");
  }
  glfwSetWindowUserPointer(mHandle, this);

  ResizeCallback DefaultResizeCallback = [this](int width, int height) {
    mWindowEventManager.PushEvent<WindowResizeEvent>(width, height);
  };

  KeyCallback DefaultKeyCallback = [this](int key, int scancode, int action, int mods) {
    switch (action) {
      case GLFW_PRESS:
        mWindowEventManager.PushEvent<KeyPressedEvent>(key, scancode, mods, false);
        break;
      case GLFW_REPEAT:
        mWindowEventManager.PushEvent<KeyPressedEvent>(key, scancode, mods, true);
        break;
      case GLFW_RELEASE:
        mWindowEventManager.PushEvent<KeyReleasedEvent>(key, scancode, mods);
        break;
      default:
        break;
    }
  };
  ScrollCallback DefaultScrollCallback = [this](double xOffset, double yOffset) {
    mWindowEventManager.PushEvent<MouseScrolledEvent>(xOffset, yOffset);
  };
  CursorCallback DefaultCursorCallback = [this](double xPos, double yPos) {
    mWindowEventManager.PushEvent<MouseMovedEvent>(xPos, yPos);
  };
  MouseButtonCallback DefaultMouseCallback = [this](int button, int action, int mods) {
    switch (action) {
      case GLFW_PRESS:
        mWindowEventManager.PushEvent<MousePressedEvent>(button, mods, false);
        break;
      case GLFW_REPEAT:
        mWindowEventManager.PushEvent<MousePressedEvent>(button, mods, true);
        break;
      case GLFW_RELEASE:
        mWindowEventManager.PushEvent<MouseReleasedEvent>(button, mods);
        break;
      default:
        break;
    }
  };

  mResizeCallback = desc.resizeCallback ? desc.resizeCallback : DefaultResizeCallback;
  mKeyCallback = desc.keyCallback ? desc.keyCallback : DefaultKeyCallback;
  mScrollCallback = desc.scrollCallback ? desc.scrollCallback : DefaultScrollCallback;
  mCursorCallback = desc.cursorCallback ? desc.cursorCallback : DefaultCursorCallback;
  mMouseButtonCallback = desc.mouseButtonCallback ? desc.mouseButtonCallback : DefaultMouseCallback;
  mRefreshCallback = desc.refreshCallback ? desc.refreshCallback : []() {
  };

  glfwSetWindowSizeCallback(mHandle, DispatchResize);
  glfwSetFramebufferSizeCallback(mHandle, DispatchResize);
  glfwSetKeyCallback(mHandle, DispatchKey);
  glfwSetScrollCallback(mHandle, DispatchScroll);
  glfwSetCursorPosCallback(mHandle, DispatchCursor);
  glfwSetMouseButtonCallback(mHandle, DispatchMouseButton);
  glfwSetWindowCloseCallback(mHandle, DispatchClose);
  glfwSetWindowRefreshCallback(mHandle, DispatchRefresh);
}

Window::~Window() {
  if (mHandle)
    glfwDestroyWindow(mHandle);
}

std::pair<int, int> Window::GetWindowSize() const {
  int w = 0, h = 0;
  glfwGetWindowSize(mHandle, &w, &h);
  return {w, h};
}
std::pair<int, int> Window::GetFramebufferSize() const {
  int w{0}, h{0};
  glfwGetFramebufferSize(mHandle, &w, &h);
  return {w, h};
}


vk::UniqueSurfaceKHR
Window::CreateWindowSurface(vk::UniqueInstance& instance,
                            const VkAllocationCallbacks* allocationCallback) const {
  VkSurfaceKHR surface;
  auto res = glfwCreateWindowSurface(*instance, mHandle, allocationCallback, &surface);
  if (res != VK_SUCCESS) {
    // TODO: Maybe give this info to caller
    Abort("Failed to create window surface");
  }
  return vk::UniqueSurfaceKHR{surface, *instance};
}
void Window::DispatchResize(GLFWwindow* window, int width, int height) {
  if (const auto* self = static_cast<Window*>(glfwGetWindowUserPointer(window));
      self && self->mResizeCallback)
    self->mResizeCallback(width, height);
}
void Window::DispatchClose(GLFWwindow* window) {
  if (auto* self = static_cast<Window*>(glfwGetWindowUserPointer(window))) {
    self->mWindowEventManager.PushEvent<WindowCloseEvent>();
    glfwSetWindowShouldClose(window, GLFW_FALSE);
  }
}
void Window::DispatchKey(GLFWwindow* window, int key, int scancode, int action, int mods) {
  if (const auto* self = static_cast<Window*>(glfwGetWindowUserPointer(window));
      self && self->mKeyCallback)
    self->mKeyCallback(key, scancode, action, mods);
}
void Window::DispatchScroll(GLFWwindow* window, double xOffset, double yOffset) {
  if (const auto* self = static_cast<Window*>(glfwGetWindowUserPointer(window));
      self && self->mScrollCallback)
    self->mScrollCallback(xOffset, yOffset);
}
void Window::DispatchCursor(GLFWwindow* window, double xPos, double yPos) {
  if (const auto* self = static_cast<Window*>(glfwGetWindowUserPointer(window));
      self && self->mCursorCallback)
    self->mCursorCallback(xPos, yPos);
}
void Window::DispatchMouseButton(GLFWwindow* window, int button, int action, int mods) {
  if (const auto* self = static_cast<Window*>(glfwGetWindowUserPointer(window));
      self && self->mMouseButtonCallback)
    self->mMouseButtonCallback(button, action, mods);
}
void Window::DispatchRefresh(GLFWwindow* window) {
  if (const auto* self = static_cast<Window*>(glfwGetWindowUserPointer(window));
      self && self->mRefreshCallback)
    self->mRefreshCallback();
}
WindowBuilder& WindowBuilder::SetTitle(const char* title) {
  mDesc.title = title;
  return *this;
}
WindowBuilder& WindowBuilder::SetSize(const int width, const int height) {
  mDesc.width = width;
  mDesc.height = height;
  return *this;
}
WindowBuilder& WindowBuilder::SetWidth(const int width) {
  mDesc.width = width;
  return *this;
}
WindowBuilder& WindowBuilder::SetHeight(const int height) {
  mDesc.height = height;
  return *this;
}
WindowBuilder& WindowBuilder::SetResizeCallback(const ResizeCallback& callback) {
  mDesc.resizeCallback = callback;
  return *this;
}
WindowBuilder& WindowBuilder::SetKeyCallback(const KeyCallback& callback) {
  mDesc.keyCallback = callback;
  return *this;
}
WindowBuilder& WindowBuilder::SetScrollCallback(const ScrollCallback& callback) {
  mDesc.scrollCallback = callback;
  return *this;
}
WindowBuilder& WindowBuilder::SetCursorCallback(const CursorCallback& callback) {
  mDesc.cursorCallback = callback;
  return *this;
}
WindowBuilder& WindowBuilder::SetMouseButtonCallback(const MouseButtonCallback& callback) {
  mDesc.mouseButtonCallback = callback;
  return *this;
}
WindowBuilder& WindowBuilder::SetRefreshCallback(const RefreshCallback& callback) {
  mDesc.refreshCallback = callback;
  return *this;
}
std::unique_ptr<Window> WindowBuilder::Build() const {
  return std::make_unique<Window>(mDesc);
}
} // namespace SD

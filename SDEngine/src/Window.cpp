#include "Window.hpp"

#include "Utils.hpp"
Window::Window(int width, int height, const std::string& title)
{
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    mHandle = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);
    if (!mHandle)
    {
        Engine::Abort("Failed to create GLFW window");
    }

    glfwSetWindowUserPointer(mHandle, this);

    glfwSetWindowSizeCallback(mHandle, DispatchResize);
    glfwSetFramebufferSizeCallback(mHandle, DispatchResize);
    glfwSetKeyCallback(mHandle, DispatchKey);
    glfwSetScrollCallback(mHandle, DispatchScroll);
    glfwSetCursorPosCallback(mHandle, DispatchCursor);
}
Window::~Window()
{
    if (mHandle)
        glfwDestroyWindow(mHandle);
}
std::pair<int, int> Window::GetWindowSize() const
{
    int w = 0, h = 0;
    glfwGetWindowSize(mHandle, &w, &h);
    return {w, h};
}
vk::UniqueSurfaceKHR Window::CreateWindowSurface(vk::UniqueInstance&          instance,
                                                 const VkAllocationCallbacks *allocationCallback) const
{
    VkSurfaceKHR surface;
    auto         res = glfwCreateWindowSurface(*instance, mHandle, allocationCallback, &surface);
    if (res != VK_SUCCESS)
    {
        // TODO: Maybe give this info to caller
        Engine::Abort("Failed to create window surface");
    }
    return vk::UniqueSurfaceKHR{surface, *instance};
}
void Window::DispatchResize(GLFWwindow *window, int width, int height)
{
    if (const auto *self = static_cast<Window *>(glfwGetWindowUserPointer(window)); self && self->mResizeCallback)
        self->mResizeCallback(width, height);
}
void Window::DispatchKey(GLFWwindow *window, int key, int scancode, int action, int mods)
{
    if (const auto *self = static_cast<Window *>(glfwGetWindowUserPointer(window)); self && self->mKeyCallback)
        self->mKeyCallback(key, scancode, action, mods);
}
void Window::DispatchScroll(GLFWwindow *window, double xOffset, double yOffset)
{
    if (const auto *self = static_cast<Window *>(glfwGetWindowUserPointer(window)); self && self->mScrollCallback)
        self->mScrollCallback(xOffset, yOffset);
}
void Window::DispatchCursor(GLFWwindow *window, double xPos, double yPos)
{
    if (const auto *self = static_cast<Window *>(glfwGetWindowUserPointer(window)); self && self->mCursorCallback)
        self->mCursorCallback(xPos, yPos);
}

#include "Window.hpp"
Window::Window(int width, int height, const std::string& title)
{
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    mHandle = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);
    if (!mHandle)
    {
        throw std::runtime_error("Failed to create GLFW window");
    }

    glfwSetWindowUserPointer(mHandle, this);

    glfwSetWindowSizeCallback(mHandle, DispatchResize);
    glfwSetFramebufferSizeCallback(mHandle, DispatchResize);
    glfwSetKeyCallback(mHandle, DispatchKey);
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
        throw std::runtime_error("Failed to create window surface");
    }
    return vk::UniqueSurfaceKHR{surface, *instance};
}
void Window::DispatchResize(GLFWwindow *window, int width, int height)
{
    auto *self = static_cast<Window *>(glfwGetWindowUserPointer(window));
    if (self && self->mResizeCallback)
        self->mResizeCallback(width, height);
}
void Window::DispatchKey(GLFWwindow *window, int key, int scancode, int action, int mods)
{
    auto *self = static_cast<Window *>(glfwGetWindowUserPointer(window));
    if (self && self->mKeyCallback)
        self->mKeyCallback(key, scancode, action, mods);
}

#include "Window.hpp"

#include "Utils.hpp"


Window::Window(int width, int height, const std::string& title)
{
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    mHandle = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);
    if (!mHandle)
    {
        const char *error = nullptr;
        glfwGetError(&error);
        if (const auto logger = spdlog::get("engine"))
            logger->error("Failed to create GLFW window with error: ", error);
        Engine::Abort("Failed to create GLFW window");
    }

    glfwSetWindowUserPointer(mHandle, this);

    // TODO: We have a bunch of callbacks, maybe we use a builder pattern so we can have some default callbacks and
    // stuff yk.
    glfwSetWindowSizeCallback(mHandle, DispatchResize);
    glfwSetFramebufferSizeCallback(mHandle, DispatchResize);
    glfwSetKeyCallback(mHandle, DispatchKey);
    glfwSetScrollCallback(mHandle, DispatchScroll);
    glfwSetCursorPosCallback(mHandle, DispatchCursor);
    glfwSetMouseButtonCallback(mHandle, DispatchMouseButton);
}

Window::Window(const WindowDesc& desc)
{
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    mHandle = glfwCreateWindow(desc.width, desc.height, desc.title, nullptr, nullptr);
    if (!mHandle)
    {
        const char *error = nullptr;
        glfwGetError(&error);
        if (const auto logger = spdlog::get("engine"))
            logger->error("Failed to create GLFW window with error: ", error);
        Engine::Abort("Failed to create GLFW window");
    }
    glfwSetWindowUserPointer(mHandle, this);

    ResizeCallback DefaultResizeCallback = [this](int, int)
    {
        resizeRequested = true;
    };

    KeyCallback DefaultKeyCallback = [this](int key, int scancode, int action, int mods)
    {
        switch (action)
        {
            case GLFW_PRESS:
                eventManager.push_back(std::make_unique<KeyPressedEvent>(key, scancode, mods, false));
                break;
            case GLFW_REPEAT:
                eventManager.push_back(std::make_unique<KeyPressedEvent>(key, scancode, mods, true));
                break;
            case GLFW_RELEASE:
                eventManager.push_back(std::make_unique<KeyReleasedEvent>(key, scancode, mods));
                break;
            default:
                break;
        }
    };
    ScrollCallback DefaultScrollCallback = [this](double xOffset, double yOffset)
    {
        eventManager.push_back(std::make_unique<ScrollEvent>(xOffset, yOffset));
    };
    CursorCallback DefaultCursorCallback = [this](double xPos, double yPos)
    {
        eventManager.push_back(std::make_unique<CursorEvent>(xPos, yPos));
    };
    MouseButtonCallback DefaultMouseCallback = [this](int button, int action, int mods)
    {
        switch (action)
        {
            case GLFW_PRESS:
                eventManager.push_back(std::make_unique<MousePressedEvent>(button, mods, false));
                break;
            case GLFW_REPEAT:
                eventManager.push_back(std::make_unique<MousePressedEvent>(button, mods, true));
                break;
            case GLFW_RELEASE:
                eventManager.push_back(std::make_unique<MouseReleasedEvent>(button, mods));
                break;
            default:
                break;
        }
    };

    mResizeCallback      = desc.resizeCallback ? desc.resizeCallback : DefaultResizeCallback;
    mKeyCallback         = desc.keyCallback ? desc.keyCallback : DefaultKeyCallback;
    mScrollCallback      = desc.scrollCallback ? desc.scrollCallback : DefaultScrollCallback;
    mCursorCallback      = desc.cursorCallback ? desc.cursorCallback : DefaultCursorCallback;
    mMouseButtonCallback = desc.mouseButtonCallback ? desc.mouseButtonCallback : DefaultMouseCallback;

    glfwSetWindowSizeCallback(mHandle, DispatchResize);
    glfwSetFramebufferSizeCallback(mHandle, DispatchResize);
    glfwSetKeyCallback(mHandle, DispatchKey);
    glfwSetScrollCallback(mHandle, DispatchScroll);
    glfwSetCursorPosCallback(mHandle, DispatchCursor);
    glfwSetMouseButtonCallback(mHandle, DispatchMouseButton);
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
void Window::DispatchMouseButton(GLFWwindow *window, int button, int action, int mods)
{
    if (const auto *self = static_cast<Window *>(glfwGetWindowUserPointer(window)); self && self->mCursorCallback)
        self->mMouseButtonCallback(button, action, mods);
}

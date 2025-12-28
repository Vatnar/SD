#pragma once

#include "VulkanConfig.hpp"
#include <functional>
#include <GLFW/glfw3.h>


class Window
{
    using ResizeCallback = std::function<void(int, int)>;
    using KeyCallback    = std::function<void(int, int, int, int)>;

public:
    Window(int width, int height, const std::string& title);

    ~Window();
    void SetResizeCallback(const ResizeCallback& callback) { mResizeCallback = callback; }
    void SetKeyCallback(const KeyCallback& callback) { mKeyCallback = callback; }

    [[nodiscard]] GLFWwindow         *GetNativeHandle() const { return mHandle; }
    [[nodiscard]] std::pair<int, int> GetWindowSize() const;
    [[nodiscard]] bool                ShouldClose() const { return glfwWindowShouldClose(mHandle); }
    vk::UniqueSurfaceKHR              CreateWindowSurface(vk::UniqueInstance&          instance,
                                                          const VkAllocationCallbacks *allocationCallback) const;

private:
    GLFWwindow    *mHandle;
    ResizeCallback mResizeCallback;
    KeyCallback    mKeyCallback;

    static void DispatchResize(GLFWwindow *window, int width, int height);
    static void DispatchKey(GLFWwindow *window, int key, int scancode, int action, int mods);
};

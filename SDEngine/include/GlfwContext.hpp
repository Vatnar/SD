#pragma once

#include "Utils.hpp"
#include "VulkanConfig.hpp"
#include "GLFW/glfw3.h"


class GlfwContext
{
public:
    GlfwContext()
    {
        if (!glfwInit())
        {
            Engine::Abort("Failed to initialise GLFW");
        }

        // Global hints
    }

    GlfwContext(const GlfwContext&)            = delete;
    GlfwContext& operator=(const GlfwContext&) = delete;

    ~GlfwContext() { glfwTerminate(); }
    static std::pair<const char **, uint32_t> GetRequiredInstanceExtensions()
    {
        uint32_t     count    = 0;
        const char **glfwExts = glfwGetRequiredInstanceExtensions(&count);
        return {glfwExts, count};
    }
};

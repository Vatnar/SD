#pragma once

#include "VulkanConfig.hpp"
#include "GLFW/glfw3.h"

#include <stdexcept>

class GlfwContext
{
public:
    GlfwContext()
    {
        if (!glfwInit())
        {
            // TODO: figure out how we do errors
            throw std::runtime_error("Failed to initialise GLFW");
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

#pragma once

#include "Utils.hpp"
#include "GLFW/glfw3.h"


class GlfwContext
{
public:
    GlfwContext()
    {
        if (glfwInit() == false)
        {
            Engine::Abort("Failed to initialise GLFW");
        }

        // Global hints
    }

    GlfwContext(const GlfwContext&)            = delete;
    GlfwContext& operator=(const GlfwContext&) = delete;

    ~GlfwContext()
    {
        if (auto logger = spdlog::get("engine"))
            logger->info("Shutting down GLFW");

        glfwTerminate();
    }
    static std::pair<const char **, uint32_t> GetRequiredInstanceExtensions()
    {
        uint32_t     count    = 0;
        const char **glfwExts = glfwGetRequiredInstanceExtensions(&count);
        return {glfwExts, count};
    }
};

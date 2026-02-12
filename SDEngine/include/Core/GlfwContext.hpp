#pragma once

#include <GLFW/glfw3.h>

#include "Utils/Utils.hpp"

using GlfwErrorCallback = std::function<void(int, const char*)>;

/**
 * @brief RAII wrapper for initializing and terminating Glfw. Also sets a glfwErrorCallback
 */
class GlfwContext {
public:
  GlfwContext() {
    // Set GLFW error callback
    sErrorCallback = [this](int errorCode, const char* description) {
      if (auto logger = spdlog::get("engine")) {
        std::string desc(description);
        logger->error("Glfw: ERROR:{}\n{}", errorCode, desc);
      }
    };
    glfwSetErrorCallback(&GlfwContext::GlfwErrorCallbackTrampoline);


    if (glfwInit() == false) {
      Engine::Abort("Failed to initialise GLFW");
    }
  }

  GlfwContext(const GlfwContext&) = delete;
  GlfwContext& operator=(const GlfwContext&) = delete;
  GlfwContext(GlfwContext&&) = delete;
  GlfwContext& operator=(GlfwContext&&) = delete;

  ~GlfwContext() {
    if (auto logger = spdlog::get("engine"))
      logger->info("Shutting down Glfw");

    glfwTerminate();
  }

  /**
   * Overides the default error callback for Glfw
   * @param callback
   */
  static void SetErrorCallback(const GlfwErrorCallback& callback) { sErrorCallback = callback; }

  static std::pair<const char**, uint32_t> GetRequiredInstanceExtensions() {
    uint32_t count = 0;
    const char** glfwExts = glfwGetRequiredInstanceExtensions(&count);
    return {glfwExts, count};
  }

private:
  static inline GlfwErrorCallback sErrorCallback;

  static void GlfwErrorCallbackTrampoline(int errorCode, const char* description) {
    sErrorCallback(errorCode, description);
  }
};

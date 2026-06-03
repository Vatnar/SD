// TODO(docs): Add file-level Doxygen header
//   - @file GlfwContext.hpp
//   - @brief RAII wrapper for GLFW initialization
//   - Error callback setup
//   - Vulkan extension query
#pragma once

#include <GLFW/glfw3.h>

#include "SD/core/base.hpp"

namespace sd {
using glfw_error_callback = std::function<void(int, const char*)>;

// TODO(docs): Document GlfwContext class
//   - Purpose: RAII wrapper for GLFW lifecycle
//   - Initialization and termination guarantees
//   - Error callback customization
//   - GetRequiredInstanceExtensions for Vulkan
//   - Example: Usage in Application
/**
 * @brief RAII wrapper for initializing and terminating Glfw. Also sets a glfwErrorCallback
 */
class SD_EXPORT GlfwContext {
public:
  GlfwContext() {
    // Set GLFW error callback
    m_error_callback = [](int error_code, const char* description) {
      log::engine::error("Glfw: ERROR:{}\n{}", error_code, description);
    };
    glfwSetErrorCallback(&GlfwContext::glfw_error_callback_trampoline);


    if (glfwInit() == false) {
      engine_abort("Failed to initialise GLFW");
    }
  }

  GlfwContext(const GlfwContext&)            = delete;
  GlfwContext& operator=(const GlfwContext&) = delete;
  GlfwContext(GlfwContext&&)                 = delete;
  GlfwContext& operator=(GlfwContext&&)      = delete;

  ~GlfwContext() {
    log::engine::info("Shutting down Glfw");

    glfwTerminate();
  }

  /**
   * Overides the default error callback for Glfw
   * @param callback
   */
  static void set_error_callback(const glfw_error_callback& callback) {
    m_error_callback = callback;
  }

  static std::pair<const char**, uint32_t> get_required_instance_extensions() {
    uint32_t     count     = 0;
    const char** glfw_exts = glfwGetRequiredInstanceExtensions(&count);
    return {glfw_exts, count};
  }

private:
  static inline glfw_error_callback m_error_callback;

  static void glfw_error_callback_trampoline(int error_code, const char* description) {
    m_error_callback(error_code, description);
  }
};
} // namespace sd

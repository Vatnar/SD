#pragma once

namespace sd {

class GlfwContext;
class VulkanContext;
class VulkanRenderer;
class SDImGuiContext;

struct EngineServices {
  GlfwContext&    glfw;
  VulkanContext&  vulkan;
  VulkanRenderer& renderer;
  SDImGuiContext& imgui;
};

} // namespace sd
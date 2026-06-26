#pragma once

namespace sd {

struct GlfwContext;
struct VulkanContext;
struct VulkanRenderer;
struct SDImGuiContext;

struct EngineServices {
  GlfwContext&    glfw;
  VulkanContext&  vulkan;
  VulkanRenderer& renderer;
  SDImGuiContext& imgui;
};

} // namespace sd

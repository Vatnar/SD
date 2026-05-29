// TODO(docs): Add file-level Doxygen header
//   - @file SDImGuiContext.hpp
//   - @brief ImGui context and Vulkan backend setup
//   - Dock space management
//   - Texture descriptor set utilities
#pragma once
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_vulkan.h>
#include <imgui.h>

#include "Core/Vulkan/VulkanContext.hpp"
#include "Core/Vulkan/VulkanWindow.hpp"
#include "Core/Window.hpp"

namespace sd {

// TODO(docs): Document SDImGuiContext class
//   - Purpose: Manages ImGui context and Vulkan integration
//   - Init/Shutdown lifecycle
//   - Frame begin/end pattern
//   - Dock space creation (BeginDockSpace/EndDockSpace)
//   - Texture utilities (CreateTextureFromView, RemoveTexture)
//   - Example: Setting up ImGui in a custom layer
class SDImGuiContext {
public:
  SDImGuiContext() = default;
  ~SDImGuiContext();

  void init(Window& window, VulkanWindow& vw);

  void shutdown();

  void begin_frame();
  void end_frame();
  void render_draw_data(vk::CommandBuffer cmd);
  void update_platform_windows();

  void begin_dock_space(const std::string& title = "SDEngine Editor");
  void end_dock_space();

  ImGuiContext* get_context() const { return m_context; }
  vk::DescriptorPool get_descriptor_pool() const { return *m_descriptor_pool; }

  VkDescriptorSet create_texture_from_view(VkImageView view, VkImageLayout layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
  void remove_texture(VkDescriptorSet descriptor_set);


private:
  void create_descriptor_pool(VulkanContext& ctx);
  void create_compatible_render_pass(VulkanContext& ctx, vk::Format format);

private:
  ImGuiContext* m_context = nullptr;

private:
  vk::UniqueDescriptorPool m_descriptor_pool;
  vk::UniqueRenderPass m_render_pass;
  vk::UniqueSampler m_default_sampler;
  bool m_is_vulkan_initialized = false;
};

} // namespace SD

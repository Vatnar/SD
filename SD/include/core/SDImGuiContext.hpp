// TODO(docs): Add file-level Doxygen header
//   - @file SDImGuiContext.hpp
//   - @brief ImGui context and Vulkan backend setup
//   - Dock space management
//   - Texture descriptor set utilities
#pragma once
#include <imgui.h>

#include <backends/imgui_impl_vulkan.h>

#include "core/Window.hpp"
#include "core/vulkan/VulkanContext.hpp"
#include "core/vulkan/VulkanWindow.hpp"

namespace sd {

struct SDImGuiCallbacks {
  std::function<void()> close_app;
};
// TODO(docs): Document SDImGuiContext class
//   - Purpose: Manages ImGui context and Vulkan integration
//   - Init/Shutdown lifecycle
//   - Frame begin/end pattern
//   - Dock space creation (BeginDockSpace/EndDockSpace)
//   - Texture utilities (CreateTextureFromView, RemoveTexture)
//   - Example: Setting up ImGui in a custom layer
class SDImGuiContext {
public:
  explicit SDImGuiContext(const SDImGuiCallbacks& callbacks) : m_callbacks(callbacks) {}
  ~SDImGuiContext();
  SDImGuiContext(const SDImGuiContext& other)                = delete;
  SDImGuiContext(SDImGuiContext&& other) noexcept            = delete;
  SDImGuiContext& operator=(const SDImGuiContext& other)     = delete;
  SDImGuiContext& operator=(SDImGuiContext&& other) noexcept = delete;


  void init(Window& window, VulkanWindow& vw);

  void shutdown();

  void begin_frame();
  void end_frame();
  void render_draw_data(vk::CommandBuffer cmd);
  void update_platform_windows();

  void begin_dock_space(const std::string& title = "SDEngine Editor");
  void end_dock_space();

  ImGuiContext*      get_context() const { return m_context; }
  vk::DescriptorPool get_descriptor_pool() const { return *m_descriptor_pool; }

  VkDescriptorSet
       create_texture_from_view(VkImageView   view,
                                VkImageLayout layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
  void remove_texture(VkDescriptorSet descriptor_set);


private:
  void create_descriptor_pool(VulkanContext& ctx);
  void create_compatible_render_pass(VulkanContext& ctx, vk::Format format);

private:
  ImGuiContext* m_context = nullptr;

  SDImGuiCallbacks m_callbacks;

  vk::UniqueDescriptorPool m_descriptor_pool;
  vk::UniqueRenderPass     m_render_pass;
  vk::UniqueSampler        m_default_sampler;
  bool                     m_is_vulkan_initialized = false;
};

} // namespace sd

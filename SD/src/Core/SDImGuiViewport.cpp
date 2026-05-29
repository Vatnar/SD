#include "Core/SDImGuiViewport.hpp"

#include <backends/imgui_impl_vulkan.h>

#include "Application.hpp"
#include "Core/SDImGuiContext.hpp"

namespace sd {

SDImGuiViewport::SDImGuiViewport(VulkanContext& ctx, const std::string& name, u32 width,
                                 u32 height) : m_name(name), m_ctx(ctx) {
  m_framebuffer = std::make_unique<VulkanFramebuffer>(ctx, width, height);

  // Create Sampler for ImGui
  vk::SamplerCreateInfo sampler_info(
      {}, vk::Filter::eLinear, vk::Filter::eLinear, vk::SamplerMipmapMode::eLinear,
      vk::SamplerAddressMode::eRepeat, vk::SamplerAddressMode::eRepeat,
      vk::SamplerAddressMode::eRepeat, 0.0f, VK_FALSE, 1.0f, VK_FALSE, vk::CompareOp::eAlways, 0.0f,
      0.0f, vk::BorderColor::eIntOpaqueBlack, VK_FALSE);
  m_sampler = check_vulkan_res_val(m_ctx.get_vulkan_device()->createSamplerUnique(sampler_info),
                               "Failed to create viewport sampler");

  // Descriptor set for ImGui
  // We need to access the descriptor pool from ImGui context
  // For now, let's assume we can get it or use a global one if available.
  // In our case, Application has mImGuiCtx.
}

SDImGuiViewport::~SDImGuiViewport() {
  // Clean up ImGui texture descriptor set
  if (m_texture_id != nullptr) {
    Application::get().get_im_gui_context().remove_texture(m_texture_id);
    m_texture_id = nullptr;
  }
}

void SDImGuiViewport::begin() {
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{0, 0});
  ImGui::Begin(m_name.c_str());

  ImVec2 viewport_panel_size = ImGui::GetContentRegionAvail();
  if (viewport_panel_size.x > 0 && viewport_panel_size.y > 0) {
    u32 width = static_cast<u32>(viewport_panel_size.x);
    u32 height = static_cast<u32>(viewport_panel_size.y);

    if (m_framebuffer->get_extent().width != width || m_framebuffer->get_extent().height != height) {
      m_framebuffer->resize(width, height);
      m_texture_id = VK_NULL_HANDLE; // Force recreation
    }
  }

  if (m_texture_id == VK_NULL_HANDLE) {
    // auto& app = Application::Get();
    // auto pool = app.GetImGuiContext().GetDescriptorPool();

    m_texture_id = ImGui_ImplVulkan_AddTexture(*m_sampler, m_framebuffer->get_color_image_view(),
                                             VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
  }

  ImGui::Image(m_texture_id, viewport_panel_size);
}

void SDImGuiViewport::end() {
  ImGui::End();
  ImGui::PopStyleVar();
}

} // namespace SD

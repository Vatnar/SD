#include "Core/SDImGuiViewport.hpp"

#include <backends/imgui_impl_vulkan.h>

#include "Application.hpp"
#include "Utils/Utils.hpp"

namespace SD {

SDImGuiViewport::SDImGuiViewport(VulkanContext& ctx, const std::string& name, u32 width,
                                 u32 height) : mName(name), mCtx(ctx) {
  mFramebuffer = std::make_unique<VulkanFramebuffer>(ctx, width, height);

  // Create Sampler for ImGui
  vk::SamplerCreateInfo samplerInfo(
      {}, vk::Filter::eLinear, vk::Filter::eLinear, vk::SamplerMipmapMode::eLinear,
      vk::SamplerAddressMode::eRepeat, vk::SamplerAddressMode::eRepeat,
      vk::SamplerAddressMode::eRepeat, 0.0f, VK_FALSE, 1.0f, VK_FALSE, vk::CompareOp::eAlways, 0.0f,
      0.0f, vk::BorderColor::eIntOpaqueBlack, VK_FALSE);
  mSampler = CheckVulkanResVal(mCtx.GetVulkanDevice()->createSamplerUnique(samplerInfo),
                               "Failed to create viewport sampler");

  // Descriptor set for ImGui
  // We need to access the descriptor pool from ImGui context
  // For now, let's assume we can get it or use a global one if available.
  // In our case, Application has mImGuiCtx.
}

SDImGuiViewport::~SDImGuiViewport() {
  // ImGui descriptor sets are usually managed by the backend or the user.
  // ImGui_ImplVulkan_RemoveTexture might not exist in all versions,
  // but usually, we don't need to manually free it if we destroy the pool.
  // However, it's good practice if supported.
}

void SDImGuiViewport::Begin() {
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{0, 0});
  ImGui::Begin(mName.c_str());

  ImVec2 viewportPanelSize = ImGui::GetContentRegionAvail();
  if (viewportPanelSize.x > 0 && viewportPanelSize.y > 0) {
    u32 width = static_cast<u32>(viewportPanelSize.x);
    u32 height = static_cast<u32>(viewportPanelSize.y);

    if (mFramebuffer->GetExtent().width != width || mFramebuffer->GetExtent().height != height) {
      mFramebuffer->Resize(width, height);
      mTextureID = VK_NULL_HANDLE; // Force recreation
    }
  }

  if (mTextureID == VK_NULL_HANDLE) {
    // auto& app = Application::Get();
    // auto pool = app.GetImGuiContext().GetDescriptorPool();

    mTextureID = ImGui_ImplVulkan_AddTexture(*mSampler, mFramebuffer->GetColorImageView(),
                                             VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
  }

  ImGui::Image((ImTextureID)mTextureID, viewportPanelSize);
}

void SDImGuiViewport::End() {
  ImGui::End();
  ImGui::PopStyleVar();
}

} // namespace SD

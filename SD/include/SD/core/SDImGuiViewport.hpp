#pragma once

#include <imgui.h>
#include <memory>
#include <string>

#include "SD/core/EngineServices.hpp"
#include "SD/core/vulkan/VulkanFramebuffer.hpp"
#include "SDImGuiContext.hpp"

namespace sd {

class SDImGuiViewport {
public:
  SDImGuiViewport(EngineServices& services, const std::string& name, uint32_t width = 1280,
                  uint32_t height = 720);
  ~SDImGuiViewport();

  void begin();
  void end();

  VulkanFramebuffer& get_framebuffer() { return *m_framebuffer; }
  const std::string& get_name() const { return m_name; }

  ImTextureID get_im_gui_texture_id() const { return reinterpret_cast<ImTextureID>(m_texture_id); }

private:
  std::string                        m_name;
  std::unique_ptr<VulkanFramebuffer> m_framebuffer;
  vk::UniqueSampler                  m_sampler;
  VkDescriptorSet                    m_texture_id = VK_NULL_HANDLE;
  VulkanContext&                     m_vulkan_ctx;
  SDImGuiContext&                    m_imgui_ctx;
};

} // namespace sd

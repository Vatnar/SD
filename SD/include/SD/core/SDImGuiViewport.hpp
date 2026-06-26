#pragma once

#include <imgui.h>
#include <string>

#include "SD/arena.hpp"
#include "SD/core/EngineServices.hpp"
#include "SD/core/vulkan/VulkanFramebuffer.hpp"
#include "SDImGuiContext.hpp"

namespace sd {

struct SDImGuiViewport {
  SDImGuiViewport(EngineServices&    services,
                  const std::string& name,
                  Arena*             arena,
                  uint32_t           width  = 1280,
                  uint32_t           height = 720);
  ~SDImGuiViewport();

  void begin();
  void end();

  VulkanFramebuffer& get_framebuffer() { return *m_framebuffer; }
  const std::string& get_name() const { return m_name; }

  ImTextureID get_im_gui_texture_id() const { return reinterpret_cast<ImTextureID>(m_texture_id); }


  std::string        m_name;
  VulkanFramebuffer* m_framebuffer;
  vk::UniqueSampler  m_sampler;
  VkDescriptorSet    m_texture_id = VK_NULL_HANDLE;
  VulkanContext&     m_vulkan_ctx;
  SDImGuiContext&    m_imgui_ctx;
};

} // namespace sd

#pragma once

#include <imgui.h>
#include <string>
#include <memory>
#include "Core/Vulkan/VulkanFramebuffer.hpp"

namespace SD {

class SDImGuiViewport {
public:
    SDImGuiViewport(VulkanContext& ctx, const std::string& name, uint32_t width = 1280, uint32_t height = 720);
    ~SDImGuiViewport();

    void Begin();
    void End();

    VulkanFramebuffer& GetFramebuffer() { return *mFramebuffer; }
    const std::string& GetName() const { return mName; }
    
    ImTextureID GetImGuiTextureID() const { return (ImTextureID)mTextureID; }

private:
    std::string mName;
    std::unique_ptr<VulkanFramebuffer> mFramebuffer;
    vk::UniqueSampler mSampler;
    VkDescriptorSet mTextureID = VK_NULL_HANDLE;
    VulkanContext& mCtx;
};

} // namespace SD

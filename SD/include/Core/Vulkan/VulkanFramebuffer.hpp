#pragma once

#include <vulkan/vulkan.hpp>
#include "Core/Vulkan/VulkanContext.hpp"

namespace SD {

class VulkanFramebuffer {
public:
    VulkanFramebuffer(VulkanContext& ctx, uint32_t width, uint32_t height);
    ~VulkanFramebuffer();

    void Resize(uint32_t width, uint32_t height);

    vk::Framebuffer GetFramebuffer() const { return *mFramebuffer; }
    vk::RenderPass GetRenderPass() const { return *mRenderPass; }
    vk::ImageView GetColorImageView() const { return *mColorImageView; }
    vk::Extent2D GetExtent() const { return mExtent; }

private:
    void CreateResources();
    void DestroyResources();

private:
    VulkanContext& mCtx;
    vk::Extent2D mExtent;

    vk::UniqueImage mColorImage;
    vk::UniqueDeviceMemory mColorImageMemory;
    vk::UniqueImageView mColorImageView;

    vk::UniqueRenderPass mRenderPass;
    vk::UniqueFramebuffer mFramebuffer;
};

} // namespace SD

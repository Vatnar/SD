#pragma once
#include "Layer.hpp"
#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_vulkan.h"
#include "VulkanContext.hpp"

class ImGuiLayer : public Layer
{
public:
    ImGuiLayer(VulkanContext& vulkanCtx, Window& window);
    ~ImGuiLayer() override;

    void OnAttach() override;
    void OnDetach() override;

    void          Begin();
    void End(uint32_t imageIndex, vk::Semaphore waitSemaphore, vk::Semaphore signalSemaphore, vk::Fence signalFence, uint32_t currentFrame);

    void OnSwapchainRecreated();

private:
    void CreateSwapchainResources();
    void DestroySwapchainResources();

    VulkanContext&      mVulkanCtx;
    Window&             mWindow;

    vk::UniqueDescriptorPool mDescriptorPool;
    vk::UniqueRenderPass     mRenderPass;

    vk::UniqueCommandPool                mCommandPool;
    std::vector<vk::UniqueCommandBuffer> mCommandBuffers;

    std::vector<vk::UniqueFramebuffer> mFramebuffers;
};

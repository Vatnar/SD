#pragma once
#include "Layer.hpp"
#include "VulkanContext.hpp"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_vulkan.h"
#include "imgui.h"

class ImGuiLayer : public Layer {
public:
  ImGuiLayer(VulkanContext& vulkanCtx, Window& window);
  ~ImGuiLayer() override;

  void OnAttach() override;
  void OnDetach() override;

  void Begin();
  void RecordCommands(uint32_t imageIndex, uint32_t currentFrame) override;
  void End();

  [[nodiscard]] vk::CommandBuffer GetCommandBuffer(uint32_t currentFrame) override {
    return *mCommandBuffers[currentFrame];
  }

  void OnSwapchainRecreated() override;

private:
  void CreateSwapchainResources();
  void DestroySwapchainResources();


  VulkanContext& mVulkanCtx;
  Window& mWindow;

  vk::UniqueDescriptorPool mDescriptorPool;
  vk::UniqueRenderPass mRenderPass;
  vk::UniqueCommandPool mCommandPool;
  std::vector<vk::UniqueCommandBuffer> mCommandBuffers;
  std::vector<vk::UniqueFramebuffer> mFramebuffers;
};

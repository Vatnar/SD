#pragma once
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_vulkan.h>
#include <imgui.h>

#include "Core/Layer.hpp"
#include "Core/Vulkan/VulkanContext.hpp"

// TODO: remove usnig namespace
using namespace SD;
class ImGuiLayer : public Layer {
public:
  ImGuiLayer(Scene& scene, VulkanContext& vulkanCtx, Window& window);
  ~ImGuiLayer() override;

  void OnAttach() override;
  void OnDetach() override;

  void Begin();
  void End();

  void OnUpdate(float dt) override;

  void OnRender(vk::CommandBuffer& cmd) override;

  void OnSwapchainRecreated() override;

private:
  void CreateCompatibleRenderPass();

  VulkanContext& mVulkanCtx;
  Window& mWindow;

  vk::UniqueDescriptorPool mDescriptorPool;
  vk::UniqueRenderPass mCompatibleRenderPass;
};

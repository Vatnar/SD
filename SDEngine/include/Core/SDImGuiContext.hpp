#pragma once
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_vulkan.h>
#include <imgui.h>

#include "Core/Vulkan/VulkanContext.hpp"
#include "Core/Vulkan/VulkanWindow.hpp"
#include "Core/Window.hpp"

namespace SD {

class SDImGuiContext {
public:
  SDImGuiContext() = default;
  ~SDImGuiContext();

  void Init(Window& window, VulkanWindow& vw);

  void Shutdown();

  void BeginFrame();
  void EndFrame();
  void RenderDrawData(vk::CommandBuffer cmd);
  void UpdatePlatformWindows();

  void BeginDockSpace(const std::string& title = "SDEngine Editor");
  void EndDockSpace();

  ImGuiContext* GetContext() const { return mContext; }
  vk::DescriptorPool GetDescriptorPool() const { return *mDescriptorPool; }


private:
  void CreateDescriptorPool(VulkanContext& ctx);
  void CreateCompatibleRenderPass(VulkanContext& ctx, vk::Format format);

private:
  ImGuiContext* mContext = nullptr;

private:
  vk::UniqueDescriptorPool mDescriptorPool;
  vk::UniqueRenderPass mRenderPass;
  bool mVulkanInitialized = false;
};

} // namespace SD

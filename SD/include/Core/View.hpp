#pragma once

#include <imgui.h>
#include <memory>
#include <string>
#include <vector>

#include "Core/LayerList.hpp"
#include "VLA/Matrix.hpp"
#include <vulkan/vulkan.hpp>
#include <vk_mem_alloc.h>

namespace SD {

using ViewId = uint32_t;

enum ViewError {
  NameAlreadyExists,
  Success,
  ViewDoesNotExist,
};

enum class AspectMode {
  FixedHeight, // -aspect to +aspect
  FixedWidth,  // -1 to +1
  BestFit      // Auto switch based on aspect
};
enum class RenderMode {
  Shaded,
  Wireframe
};

class View {
public:
  explicit View(const std::string& name) : mName(name) {
    mCameraViewProjection = VLA::Matrix4x4f::Ortho(-1.0f, 1.0f, -1.0f, 1.0f, -1.0f, 1.0f);
  }
  virtual ~View() = default;

  void OnUpdate(float dt) { mLayers.Update(dt); }
  virtual void OnGuiRender();

  virtual void OnEvent(Event& e) { mLayers.OnEvent(e); }
  virtual void OnRender(vk::CommandBuffer cmd);
  virtual void OnFixedUpdate(double dt) { mLayers.OnFixedUpdate(dt); }

  // --- Layer management ---
  template<typename T, typename... Args>
    requires std::is_base_of_v<Layer, T>
  T& PushLayer(Args&&... args) {
    return mLayers.PushLayer<T>(std::forward<Args>(args)...);
  }

  template<typename T, typename... Args>
    requires std::is_base_of_v<Layer, T>
  T& PushLayer(int stageOrder, Args&&... args) {
    auto layer = std::make_unique<T>(std::forward<Args>(args)...);
    layer->mStageId = stageOrder;
    layer->mViewId = mViewId;
    layer->mView = this;

    if (stageOrder >= mLayersByStage.size())
      mLayersByStage.resize(stageOrder + 1);
    mLayersByStage[stageOrder] = std::move(layer);
    return static_cast<T&>(*mLayersByStage[stageOrder]);
  }

  // --- Identity ---
  const std::string& GetName() const { return mName; }
  [[nodiscard]] u32 ViewId() const { return mViewId; }
  [[nodiscard]] bool IsOpen() const { return mOpen; }
  void SetOpen(bool open) { mOpen = open; }

  // --- Layers ---
  LayerList& GetLayers() { return mLayers; }
  const LayerList& GetLayers() const { return mLayers; }

  // --- Viewport state ---
  VkExtent2D GetExtent() const { return mExtent; }
  void Resize(VkExtent2D extent);

  AspectMode GetAspectMode() const { return mAspectMode; }
  void SetAspectMode(AspectMode mode) { mAspectMode = mode; Resize(mExtent); }

  RenderMode GetRenderMode() const { return mRenderMode; }
  void SetRenderMode(RenderMode mode) { mRenderMode = mode; }

  const VLA::Matrix4x4f& GetProjection() const { return mCameraViewProjection; }

  /// Returns true (once) if the extent changed since the last call.
  bool ConsumeExtentChanged() {
    bool changed = mExtentChanged;
    mExtentChanged = false;
    return changed;
  }

  // --- Diagnostics (ImGui region tracking) ---
  ImVec2 GetContentRegionPos() const { return mContentRegionPos; }
  ImVec2 GetContentRegionExtent() const { return mContentRegionExtent; }

  // --- Rendering setup ---
  static VkExtent2D GetImGuiExtent();
  VkFormat FindDepthFormat();
  [[nodiscard]] vk::RenderPass GetLayeredRenderPass() const { return mLayeredRP.get(); }
  Layer* GetLayerByStage(u32 stage);

  void SetupLayeredRender(u32 maxStages, VkExtent2D initialExtent = {1280, 720});
  void CleanupLayeredRender();

private:
  void CreateVulkanResources();

  std::string mName;
  LayerList mLayers;
  bool mOpen = true;
  u32 mViewId = 0;

  // ImGui content region (updated each frame in OnGuiRender)
  ImVec2 mContentRegionPos{0, 0};
  ImVec2 mContentRegionExtent{0, 0};

  // Vulkan rendering resources
  vk::UniqueRenderPass mLayeredRP;
  
  VkImage mColorImage = VK_NULL_HANDLE;
  VmaAllocation mColorAllocation = VK_NULL_HANDLE;
  vk::UniqueImageView mColorView;
  
  VkImage mDepthImage = VK_NULL_HANDLE;
  VmaAllocation mDepthAllocation = VK_NULL_HANDLE;
  vk::UniqueImageView mDepthView;

  VkDescriptorSet mDisplayTexDS = VK_NULL_HANDLE; // imgui::image
  
  vk::UniqueFramebuffer mLayeredFramebuffer;

  std::vector<std::unique_ptr<Layer>> mLayersByStage;
  VLA::Matrix4x4f mCameraViewProjection;
  VkExtent2D mExtent = {1280, 720};
  bool mExtentChanged = false;

  AspectMode mAspectMode = AspectMode::BestFit;
  RenderMode mRenderMode = RenderMode::Shaded;

  friend class Application; // needs mViewId assignment
  friend class ViewManager;
};


} // namespace SD

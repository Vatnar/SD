#pragma once

#include "Events/Event.hpp"
#include "Scene.hpp"
#include "Vulkan/VulkanConfig.hpp"
#include "types.hpp"


namespace SD {
class View;
class Layer;
template<typename T> concept IsLayer = std::is_base_of_v<Layer, T>;

/// Base class for all layers. Prefer using System, RenderStage, or Panel instead.
class Layer {
public:
  virtual ~Layer() = default;

  explicit Layer(const std::string& name = "Layer", Scene* scene = nullptr) :
    mDebugName(name), mScene(scene), mStageId(0), mViewId(0) {}

  virtual void OnAttach() {}
  virtual void OnDetach() {}

  virtual void OnActivate() {}
  virtual void OnDeactivate() {}

  virtual void OnSwapchainRecreated() {}

  const std::string& GetName() const { return mDebugName; }

  void SetScene(Scene* scene) { mScene = scene; }
  [[nodiscard]] Scene* GetScene() const { return mScene; }

  virtual void OnEvent(Event&) {}
  virtual void OnFixedUpdate(double fixedDelta) {}
  virtual void OnUpdate(float dt) {}
  virtual void OnRender(vk::CommandBuffer cmd) {}
  virtual void OnGuiRender() {}
  virtual void OnImGuiMenuBar() {}

  [[nodiscard]] bool IsActive() const noexcept { return mActive; }

protected:
  bool mActive = true;
  std::string mDebugName;
  Scene* mScene = nullptr;
  int mStageId;
  u32 mViewId;
  View* mView = nullptr;

  friend class View;
};

// ---------------------------------------------------------------------------
// Focused layer subtypes — use these instead of raw Layer
// ---------------------------------------------------------------------------

/// Logic-only layer: events, update, fixed update. No rendering or UI.
class System : public Layer {
public:
  using Layer::Layer;

  // --- Available for override ---
  // OnEvent, OnUpdate, OnFixedUpdate, OnAttach, OnDetach

  // --- Sealed (not for Systems) ---
  void OnRender(vk::CommandBuffer) final {}
  void OnGuiRender() final {}
  void OnImGuiMenuBar() final {}
  void OnSwapchainRecreated() final {}
};

/// GPU command recording layer: bound to a View + render stage. No logic or UI.
class RenderStage : public Layer {
public:
  using Layer::Layer;

  // --- Available for override ---
  // OnRender, OnSwapchainRecreated, OnAttach, OnDetach

  // --- Sealed (not for RenderStages) ---
  void OnUpdate(float) final {}
  void OnFixedUpdate(double) final {}
  void OnGuiRender() final {}
  void OnImGuiMenuBar() final {}
};

/// ImGui UI layer: panels, inspectors, debug tools. No GPU rendering.
class Panel : public Layer {
public:
  using Layer::Layer;

  // --- Available for override ---
  // OnGuiRender, OnImGuiMenuBar, OnUpdate, OnEvent, OnAttach, OnDetach

  // --- Sealed (not for Panels) ---
  void OnRender(vk::CommandBuffer) final {}
  void OnSwapchainRecreated() final {}
};

} // namespace SD

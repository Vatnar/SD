#pragma once

#include "Events/Event.hpp"
#include "Scene.hpp"
#include "Vulkan/VulkanConfig.hpp"


namespace SD {
class Layer;
template<typename T> concept IsLayer = std::is_base_of_v<Layer, T>;

class Layer {
public:
  virtual ~Layer() = default;

  explicit Layer(const std::string& name = "Layer", Scene* scene = nullptr) : mDebugName(name), mScene(scene) {}

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

  [[nodiscard]] bool IsActive() const noexcept { return mActive; }

protected:
  bool mActive{true};
  std::string mDebugName;
  Scene* mScene{nullptr};
};
} // namespace SD

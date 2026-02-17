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
  explicit Layer(Scene& scene) : mActiveScene(scene) {}

  virtual void OnAttach() {}
  virtual void OnDetach() {}

  virtual void OnActivate() {}
  virtual void OnDeactivate() {}

  virtual void OnEvent(Event&) {}
  virtual void OnSwapchainRecreated() {}

  virtual void OnRender(vk::CommandBuffer& cmd);

  virtual void OnUpdate(float dt);

  [[nodiscard]] bool IsActive() const noexcept { return mActive; }

protected:
  bool mActive{true};
  Scene& mActiveScene;
};
} // namespace SD

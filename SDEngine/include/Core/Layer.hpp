#pragma once
#include <concepts>
#include <cstdint>

#include "Core/Events/InputEvent.hpp"
#include "VulkanConfig.hpp"


class Layer;
template<typename T> concept IsLayer = std::derived_from<T, Layer>;

class Layer {
public:
  virtual ~Layer() = default;
  Layer() = default;

  virtual void OnAttach() {}
  virtual void OnDetach() {}

  virtual void OnActivate() {}
  virtual void OnDeactivate() {}

  virtual void OnEvent(InputEvent&) {}
  virtual void OnSwapchainRecreated() {}

  virtual void OnRender();
  virtual void RecordCommands(uint32_t imageIndex, uint32_t currentFrame);
  virtual vk::CommandBuffer GetCommandBuffer(uint32_t currentFrame);
  virtual void OnUpdate(float dt);

  [[nodiscard]] bool IsActive() const noexcept { return mActive; }

protected:
  bool mActive{true};
};

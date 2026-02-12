#pragma once
#include <concepts>
#include <cstdint>

#include "Core/Events/InputEvent.hpp"
#include "VulkanConfig.hpp"


class Layer;
template<typename T> concept IsLayer = std::derived_from<T, Layer>;

// TODO: BIG REFACTOR - Decouple Rendering from Layers
// The current design tightly couples rendering logic (Vulkan commands, pipeline management, etc.)
// with the Layer system. This makes layers "heavy" and less modular. A better approach would be to
// move towards a more data-oriented design where layers primarily manage game logic and scene data,
// and a separate rendering system handles the drawing.
//
// How to do it:
// 1. Create a central "Renderer" or "RenderSystem" class. This class would manage the main command
// buffers, render passes, and frame submission.
// 2. Layers should not directly create or manage Vulkan resources like pipelines, descriptor sets,
// or command buffers.
// 3. Instead of `RecordCommands`, layers could have a method like
// `OnCollectRenderables(RenderQueue& queue)` or `Submit(Renderer& renderer)`.
// 4. Inside this method, layers would submit "Renderables" or "DrawCommands" to the renderer. These
// are simple data structures
//    containing information needed for drawing (e.g., mesh, material/shader, transform, textures).
// 5. The Renderer would then process this queue of renderables, sort them (e.g., by pipeline,
// material, or depth), and record the necessary Vulkan commands.
//
// Example:
// class Layer {
//   ...
//   virtual void OnUpdate(float dt) {
//     // Update game logic, transforms, etc.
//   }
//   virtual void Submit(Renderer& renderer) {
//     renderer.Submit(m_MyRenderableObject); // m_MyRenderableObject contains mesh, material,
//     transform
//   }
//   ...
// };
//
// This would make layers much thinner and focused on layer logic, while centralizing the complex
// rendering logic in one place.
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
  // TODO: Consider passing a CommandBuffer directly instead of imageIndex/currentFrame to decouple
  // recording from frame management
  virtual void RecordCommands(uint32_t imageIndex, uint32_t currentFrame);
  virtual vk::CommandBuffer GetCommandBuffer(uint32_t currentFrame);
  virtual void OnUpdate(float dt);

  [[nodiscard]] bool IsActive() const noexcept { return mActive; }

protected:
  bool mActive{true};
};

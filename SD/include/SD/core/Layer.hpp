// TODO(docs): Add file-level Doxygen header
//   - @file Layer.hpp
//   - @brief Layer system architecture - the core abstraction for engine functionality
//   - Explain the three layer subtypes: System, RenderStage, Panel
//   - Layer lifecycle and event flow
//   - When to use each subtype
#pragma once

#include <vulkan/vulkan.hpp>

#include "SD/core/id_types.hpp"
#include "SD/export.hpp"
#include "Scene.hpp"
#include "events/Event.hpp"

namespace sd {
class View;
class Layer;
template<typename T> concept IsLayer = std::is_base_of_v<Layer, T>;

// TODO(docs): Document Layer base class
//   - Purpose: Base abstraction for all game/engine functionality
//   - Virtual methods and when to override each
//   - mActive flag usage
//   - Scene association
//   - Example custom layer implementation
/// Base class for all layers. Prefer using System, RenderStage, or Panel instead.
class SD_EXPORT Layer {
public:
  virtual ~Layer() = default;

  explicit Layer(const std::string& name = "Layer", Scene* scene = nullptr) :
    m_debug_name(name), m_scene(scene), m_stage_id(0), m_view_id{} {}

  virtual void on_attach() {}
  virtual void on_detach() {}

  virtual void on_activate() {}
  virtual void on_deactivate() {}

  virtual void on_swapchain_recreated() {}

  const std::string& get_name() const { return m_debug_name; }

  void                 set_scene(Scene* scene) { m_scene = scene; }
  [[nodiscard]] Scene* get_scene() const { return m_scene; }

  virtual void on_event(Event&) {}
  virtual void on_fixed_update([[maybe_unused]] double fixedDelta) {}
  virtual void on_update([[maybe_unused]] float dt) { (void)dt; }
  virtual void on_render([[maybe_unused]] vk::CommandBuffer cmd) {}
  virtual void on_gui_render() {}
  virtual void on_im_gui_menu_bar() {}

  [[nodiscard]] bool is_active() const noexcept { return m_is_active; }

protected:
  bool        m_is_active = true;
  std::string m_debug_name;
  Scene*      m_scene = nullptr;
  int         m_stage_id;
  ViewId      m_view_id;
  View*       m_view = nullptr;

  friend class View;
};

// ---------------------------------------------------------------------------
// Focused layer subtypes — use these instead of raw Layer
// ---------------------------------------------------------------------------

/// Logic-only layer: events, update, fixed update. No rendering or UI.
class SD_EXPORT System : public Layer {
public:
  using Layer::Layer;

  // --- Available for override ---
  // OnEvent, OnUpdate, OnFixedUpdate, OnAttach, OnDetach

  // --- Sealed (not for Systems) ---
  void on_render(vk::CommandBuffer) final {}
  void on_gui_render() final {}
  void on_im_gui_menu_bar() final {}
  void on_swapchain_recreated() final {}
};

/// GPU command recording layer: bound to a View + render stage. No logic or UI.
class SD_EXPORT RenderStage : public Layer {
public:
  using Layer::Layer;

  // --- Available for override ---
  // OnRender, OnSwapchainRecreated, OnAttach, OnDetach

  // --- Sealed (not for RenderStages) ---
  void on_update(float) final {}
  void on_fixed_update(double) final {}
  void on_gui_render() final {}
  void on_im_gui_menu_bar() final {}
};

/// ImGui UI layer: panels, inspectors, debug tools. No GPU rendering.
class SD_EXPORT Panel : public Layer {
public:
  using Layer::Layer;

  // --- Available for override ---
  // OnGuiRender, OnImGuiMenuBar, OnUpdate, OnEvent, OnAttach, OnDetach

  // --- Sealed (not for Panels) ---
  void on_render(vk::CommandBuffer) final {}
  void on_swapchain_recreated() final {}
};

} // namespace sd

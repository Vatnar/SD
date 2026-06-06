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
class Application;
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
  bool         m_is_active = true;
  std::string  m_debug_name;
  Scene*       m_scene = nullptr;
  Application* m_app   = nullptr;
  int          m_stage_id;
  ViewId       m_view_id;
  View*        m_view = nullptr;

  friend class View;
  friend class Application;
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

  // --- Default no-ops (override if needed) ---
  void on_render(vk::CommandBuffer) override {}
  void on_gui_render() override {}
  void on_im_gui_menu_bar() override {}
  void on_swapchain_recreated() override {}
};

/// GPU command recording layer: bound to a View + render stage.
class SD_EXPORT RenderStage : public Layer {
public:
  using Layer::Layer;

  // --- Default no-ops (override if needed) ---
  void on_update(float) override {}
  void on_fixed_update(double) override {}
  void on_gui_render() override {}
  void on_im_gui_menu_bar() override {}
};

/// ImGui UI layer: panels, inspectors, debug tools.
class SD_EXPORT Panel : public Layer {
public:
  using Layer::Layer;

  // --- Default no-ops (override if needed) ---
  void on_render(vk::CommandBuffer) override {}
  void on_swapchain_recreated() override {}
};

} // namespace sd

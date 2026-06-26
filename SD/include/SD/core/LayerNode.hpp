#pragma once

#include <vulkan/vulkan.hpp>

#include "SD/core/events/EventVariant.hpp"
#include "SD/core/id_types.hpp"
#include "SD/core/types.hpp"

namespace sd {

struct Application;
struct Scene;
struct View;

struct LayerNode {
  void* data    = nullptr;
  U64   type_id = 0;

  bool         is_active  = true;
  const char*  debug_name = nullptr;
  Scene*       scene      = nullptr;
  Application* app        = nullptr;
  int          stage_id   = 0;
  ViewId       view_id    = ViewId{0};
  View*        view       = nullptr;

  void (*on_attach_fn)(void*)                    = nullptr;
  void (*on_detach_fn)(void*)                    = nullptr;
  void (*on_activate_fn)(void*)                  = nullptr;
  void (*on_deactivate_fn)(void*)                = nullptr;
  void (*on_event_fn)(void*, EventVariant&)      = nullptr;
  void (*on_update_fn)(void*, float)             = nullptr;
  void (*on_fixed_update_fn)(void*, double)      = nullptr;
  void (*on_render_fn)(void*, vk::CommandBuffer) = nullptr;
  void (*on_gui_render_fn)(void*)                = nullptr;
  void (*on_im_gui_menu_bar_fn)(void*)           = nullptr;
  void (*on_swapchain_recreated_fn)(void*)       = nullptr;
  void (*on_shader_reload_fn)(void*)             = nullptr;

  void on_attach() const {
    if (on_attach_fn)
      on_attach_fn(data);
  }
  void on_detach() const {
    if (on_detach_fn)
      on_detach_fn(data);
  }
  void on_activate() const {
    if (on_activate_fn)
      on_activate_fn(data);
  }
  void on_deactivate() const {
    if (on_deactivate_fn)
      on_deactivate_fn(data);
  }
  void on_event(EventVariant& e) const {
    if (on_event_fn)
      on_event_fn(data, e);
  }
  void on_update(float dt) const {
    if (on_update_fn)
      on_update_fn(data, dt);
  }
  void on_fixed_update(double dt) const {
    if (on_fixed_update_fn)
      on_fixed_update_fn(data, dt);
  }
  void on_render(vk::CommandBuffer cmd) const {
    if (on_render_fn)
      on_render_fn(data, cmd);
  }
  void on_gui_render() const {
    if (on_gui_render_fn)
      on_gui_render_fn(data);
  }
  void on_im_gui_menu_bar() const {
    if (on_im_gui_menu_bar_fn)
      on_im_gui_menu_bar_fn(data);
  }
  void on_swapchain_recreated() const {
    if (on_swapchain_recreated_fn)
      on_swapchain_recreated_fn(data);
  }
  void on_shader_reload() const {
    if (on_shader_reload_fn)
      on_shader_reload_fn(data);
  }
};

} // namespace sd

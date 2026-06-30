#pragma once

#include <vulkan/vulkan.hpp>

#include "LayerNode.hpp"
#include "SD/arena.hpp"
#include "SD/core/arena_vec.hpp"
#include "SD/core/type_id.hpp"
#include "SD/export.hpp"

namespace sd {

struct Layer;

struct SD_EXPORT LayerList {
  LayerList() {
    m_arena = arena_alloc(ArenaParams{.reserve_size = kb(64uz), .name = "LayerListArena"});
  }
  ~LayerList() {
    if (m_arena)
      arena_release(m_arena);
  }
  LayerList(const LayerList&)            = delete;
  LayerList& operator=(const LayerList&) = delete;
  LayerList(LayerList&& other) noexcept : m_arena(other.m_arena), m_layers(other.m_layers) {
    other.m_arena = nullptr;
    other.m_layers.clear();
  }
  LayerList& operator=(LayerList&& other) noexcept {
    if (this != &other) {
      if (m_arena)
        arena_release(m_arena);
      m_arena       = other.m_arena;
      m_layers      = other.m_layers;
      other.m_arena = nullptr;
      other.m_layers.clear();
    }
    return *this;
  }

  using iterator       = LayerNode*;
  using const_iterator = const LayerNode*;

  iterator       begin() { return m_layers.begin(); }
  iterator       end() { return m_layers.end(); }
  const_iterator begin() const { return m_layers.begin(); }
  const_iterator end() const { return m_layers.end(); }

  template<typename T, typename... Args>
    requires std::is_base_of_v<Layer, T>
  T& push_layer(Args&&... args) {
    T* data = m_arena->push_array<T>(1);
    if constexpr (!std::is_trivially_constructible_v<T, Args...>)
      new (data) T(std::forward<Args>(args)...);

    LayerNode node;
    node.data       = data;
    node.type_id    = type_id_of<T>();
    node.is_active  = data->is_active;
    node.debug_name = data->debug_name;
    node.scene      = data->scene;
    node.app        = data->app;
    node.stage_id   = data->stage_id;
    node.view_id    = data->view_id;
    node.view       = data->view;

    setup_fn_ptrs<T>(node);

    m_layers.push(m_arena, node);
    node.on_attach();
    return *data;
  }

  template<typename T, typename... Args>
    requires std::is_base_of_v<Layer, T>
  T& push_bottom(Args&&... args) {
    T* data = m_arena->push_array<T>(1);
    if constexpr (!std::is_trivially_constructible_v<T, Args...>)
      new (data) T(std::forward<Args>(args)...);

    LayerNode node;
    node.data       = data;
    node.type_id    = type_id_of<T>();
    node.is_active  = data->is_active;
    node.debug_name = data->debug_name;
    node.scene      = data->scene;
    node.app        = data->app;
    node.stage_id   = data->stage_id;
    node.view_id    = data->view_id;
    node.view       = data->view;

    setup_fn_ptrs<T>(node);

    m_layers.push(m_arena, node);
    node.on_attach();
    return *data;
  }

  template<typename T>
    requires std::is_base_of_v<Layer, T>
  T* Get() {
    U64 tid = type_id_of<T>();
    for (U64 i = 0; i < m_layers.count; ++i) {
      if (m_layers[i].type_id == tid)
        return static_cast<T*>(m_layers[i].data);
    }
    return nullptr;
  }

  template<typename T>
    requires std::is_base_of_v<Layer, T>
  T* pop_layer() {
    U64 tid = type_id_of<T>();
    for (I64 i = static_cast<I64>(m_layers.count) - 1; i >= 0; --i) {
      if (m_layers[i].type_id == tid) {
        m_layers[i].on_detach();
        T* result = static_cast<T*>(m_layers[i].data);
        for (U64 j = static_cast<U64>(i); j < m_layers.count - 1; ++j)
          m_layers[j] = m_layers[j + 1];
        m_layers.count--;
        return result;
      }
    }
    return nullptr;
  }

  void clear() {
    for (I64 i = static_cast<I64>(m_layers.count) - 1; i >= 0; --i) {
      m_layers[i].on_detach();
      if (m_layers[i].destroy_fn)
        m_layers[i].destroy_fn(m_layers[i].data);
    }
    m_layers.clear();
    if (m_arena)
      m_arena->clear();
  }

  void update(float dt) const {
    for (U64 i = 0; i < m_layers.count; ++i) {
      if (m_layers[i].is_active)
        m_layers[i].on_update(dt);
    }
  }

  void on_fixed_update(double dt) const {
    for (U64 i = 0; i < m_layers.count; ++i) {
      if (m_layers[i].is_active)
        m_layers[i].on_fixed_update(dt);
    }
  }

  void gui_render() const {
    for (U64 i = 0; i < m_layers.count; ++i) {
      if (m_layers[i].is_active)
        m_layers[i].on_gui_render();
    }
  }

  void on_imGui_menu_bar() const {
    for (U64 i = 0; i < m_layers.count; ++i) {
      if (m_layers[i].is_active)
        m_layers[i].on_im_gui_menu_bar();
    }
  }

  void on_event(EventVariant& e) {
    for (I64 i = static_cast<I64>(m_layers.count) - 1; i >= 0; --i) {
      if (m_layers[i].is_active) {
        m_layers[i].on_event(e);
        if (e.handled)
          break;
      }
    }
  }

  void on_swapchain_recreated() const {
    for (U64 i = 0; i < m_layers.count; ++i) {
      if (m_layers[i].is_active)
        m_layers[i].on_swapchain_recreated();
    }
  }

  void on_shader_reload() const {
    for (U64 i = 0; i < m_layers.count; ++i) {
      if (m_layers[i].is_active)
        m_layers[i].on_shader_reload();
    }
  }

  void on_render(vk::CommandBuffer cmd) const {
    for (U64 i = 0; i < m_layers.count; ++i) {
      if (m_layers[i].is_active)
        m_layers[i].on_render(cmd);
    }
  }

  U64 size() const { return m_layers.count; }


  template<typename T>
  static void setup_fn_ptrs(LayerNode& node) {
    if constexpr (!std::is_trivially_destructible_v<T>)
      node.destroy_fn = [](void* d) {
        static_cast<T*>(d)->~T();
      };
    if constexpr (requires(T& t, EventVariant& e) { t.on_event(e); })
      node.on_event_fn = [](void* d, EventVariant& e) {
        static_cast<T*>(d)->on_event(e);
      };
    if constexpr (requires(T& t, float dt) { t.on_update(dt); })
      node.on_update_fn = [](void* d, float dt) {
        static_cast<T*>(d)->on_update(dt);
      };
    if constexpr (requires(T& t, double dt) { t.on_fixed_update(dt); })
      node.on_fixed_update_fn = [](void* d, double dt) {
        static_cast<T*>(d)->on_fixed_update(dt);
      };
    if constexpr (requires(T& t, vk::CommandBuffer cmd) { t.on_render(cmd); })
      node.on_render_fn = [](void* d, vk::CommandBuffer cmd) {
        static_cast<T*>(d)->on_render(cmd);
      };
    if constexpr (requires(T& t) { t.on_gui_render(); })
      node.on_gui_render_fn = [](void* d) {
        static_cast<T*>(d)->on_gui_render();
      };
    if constexpr (requires(T& t) { t.on_im_gui_menu_bar(); })
      node.on_im_gui_menu_bar_fn = [](void* d) {
        static_cast<T*>(d)->on_im_gui_menu_bar();
      };
    if constexpr (requires(T& t) { t.on_attach(); })
      node.on_attach_fn = [](void* d) {
        static_cast<T*>(d)->on_attach();
      };
    if constexpr (requires(T& t) { t.on_detach(); })
      node.on_detach_fn = [](void* d) {
        static_cast<T*>(d)->on_detach();
      };
    if constexpr (requires(T& t) { t.on_activate(); })
      node.on_activate_fn = [](void* d) {
        static_cast<T*>(d)->on_activate();
      };
    if constexpr (requires(T& t) { t.on_deactivate(); })
      node.on_deactivate_fn = [](void* d) {
        static_cast<T*>(d)->on_deactivate();
      };
    if constexpr (requires(T& t) { t.on_swapchain_recreated(); })
      node.on_swapchain_recreated_fn = [](void* d) {
        static_cast<T*>(d)->on_swapchain_recreated();
      };
    if constexpr (requires(T& t) { t.on_shader_reload(); })
      node.on_shader_reload_fn = [](void* d) {
        static_cast<T*>(d)->on_shader_reload();
      };
  }

  Arena*              m_arena = nullptr;
  ArenaVec<LayerNode> m_layers;
};

} // namespace sd

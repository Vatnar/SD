// TODO(docs): Add file-level Doxygen header
//   - @file LayerList.hpp
//   - @brief Ordered collection of layers with lifecycle management
//   - Layer ordering and event propagation
#pragma once

#include "Core/Layer.hpp"

namespace sd {
// TODO(docs): Document LayerList class
//   - Purpose: Manages an ordered collection of layers
//   - Layer lifecycle (PushLayer -> OnAttach, PopLayer -> OnDetach)
//   - Event propagation order (reverse iteration for bubbling)
//   - Iteration patterns
//   - Thread safety considerations
class LayerList

{
public:
  LayerList() = default;
  LayerList(const LayerList&) = delete;
  LayerList& operator=(const LayerList&) = delete;

  LayerList(LayerList&&) noexcept = default;
  LayerList& operator=(LayerList&&) noexcept = default;
  using iterator = std::vector<std::unique_ptr<Layer>>::iterator;
  using const_iterator = std::vector<std::unique_ptr<Layer>>::const_iterator;

  iterator begin() { return m_layers.begin(); }
  iterator end() { return m_layers.end(); }
  [[nodiscard]] const_iterator begin() const { return m_layers.begin(); }
  [[nodiscard]] const_iterator end() const { return m_layers.end(); }

  ~LayerList() {
    // Clear in reverse order (opposite of push)
    for (auto it = m_layers.rbegin(); it != m_layers.rend(); ++it) {
      if (*it) {
        (*it)->on_detach();
      }
    }
  }

  void update(const float dt) const {
    std::ranges::for_each(m_layers, [dt](const auto& layer) {
      if (layer->is_active())
        layer->on_update(dt);
    });
  }
  void on_fixed_update(double dt) const {
    std::ranges::for_each(m_layers, [dt](const auto& layer) {
      if (layer->is_active())
        layer->on_fixed_update(dt);
    });
  }
  void gui_render() {
    std::ranges::for_each(m_layers, [](const auto& layer) {
      if (layer->is_active())
        layer->on_gui_render();
    });
  }
  void on_imGui_menu_bar() {
    std::ranges::for_each(m_layers, [](const auto& layer) {
      if (layer->is_active())
        layer->on_im_gui_menu_bar();
    });
  }

  void on_event(Event& e) {
    for (auto it = m_layers.rbegin(); it != m_layers.rend(); ++it) {
      if ((*it)->is_active()) {
        (*it)->on_event(e);
        if (e.m_handled)
          break;
      }
    }
  }

  template<IsLayer T, typename... Args>
  T& push_layer(Args&&... args) {
    auto layer = std::make_unique<T>(std::forward<Args>(args)...);
    T* ptr = layer.get();

    m_layers.emplace_back(std::move(layer));
    ptr->on_attach();

    return *ptr;
  }

  template<IsLayer T, typename... Args>
  T& push_bottom(Args&&... args) {
    auto layer = std::make_unique<T>(std::forward<Args>(args)...);
    T* ptr = layer.get();

    // Vector specific: insert at begin
    m_layers.insert(m_layers.begin(), std::move(layer));
    ptr->on_attach();

    return *ptr;
  }

  template<IsLayer T>
  T& attach_layer(std::unique_ptr<T> layer) {
    T* ptr = layer.get();
    m_layers.emplace_back(std::move(layer));
    ptr->on_attach();
    return *ptr;
  }
  template<IsLayer T>
  T* Get() {
    for (auto& layer : m_layers) {
      // dynamic_cast is safe here, but ensuring T is final/sealed helps compiler optimization
      if (auto p = dynamic_cast<T*>(layer.get())) {
        return p;
      }
    }
    return nullptr;
  }

  template<IsLayer T>
  std::unique_ptr<T> pop_layer() {
    auto it = std::find_if(m_layers.begin(), m_layers.end(),
                           [](const auto& l) { return dynamic_cast<T*>(l.get()) != nullptr; });

    if (it != m_layers.end()) {
      (*it)->on_detach();

      std::unique_ptr<T> result(static_cast<T*>(it->release()));
      m_layers.erase(it);
      return result;
    }
    return nullptr;
  }

  void clear() {
    // Clear in reverse order (opposite of push)
    for (auto it = m_layers.rbegin(); it != m_layers.rend(); ++it) {
      if (*it) {
        (*it)->on_detach();
      }
    }
    m_layers.clear();
  }

  void on_swapchain_recreated() {
    std::ranges::for_each(m_layers, [](const auto& layer) {
      if (layer->is_active())
        layer->on_swapchain_recreated();
    });
  }

  void on_render(vk::CommandBuffer& cmd) const {
    std::ranges::for_each(m_layers, [&cmd](const auto& layer) {
      if (layer->is_active())
        layer->on_render(cmd);
    });
  }

private:
  std::vector<std::unique_ptr<Layer>> m_layers{};
};
} // namespace SD

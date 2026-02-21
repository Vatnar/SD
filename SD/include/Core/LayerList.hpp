#pragma once
#include <list>
#include <spdlog/spdlog.h>

#include "Core/Layer.hpp"

namespace SD {
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

  iterator begin() { return mLayers.begin(); }
  iterator end() { return mLayers.end(); }
  [[nodiscard]] const_iterator begin() const { return mLayers.begin(); }
  [[nodiscard]] const_iterator end() const { return mLayers.end(); }

  ~LayerList() {
    std::ranges::for_each(mLayers, [](const auto& layer) { layer->OnDetach(); });
  }

  void Update(const float dt) const {
    std::ranges::for_each(mLayers, [dt](const auto& layer) {
      if (layer->IsActive())
        layer->OnUpdate(dt);
    });
  }
  void OnFixedUpdate(double dt) const {
    std::ranges::for_each(mLayers, [dt](const auto& layer) {
      if (layer->IsActive())
        layer->OnFixedUpdate(dt);
    });
  }
  void GuiRender() {
    std::ranges::for_each(mLayers, [](const auto& layer) {
      if (layer->IsActive())
        layer->OnGuiRender();
    });
  }
  void OnImGuiMenuBar() {
    std::ranges::for_each(mLayers, [](const auto& layer) {
      if (layer->IsActive())
        layer->OnImGuiMenuBar();
    });
  }

  void OnEvent(Event& e) {
    for (auto it = mLayers.rbegin(); it != mLayers.rend(); ++it) {
      if ((*it)->IsActive()) {
        (*it)->OnEvent(e);
        if (e.isHandled)
          break;
      }
    }
  }

  template<IsLayer T, typename... Args>
  T& PushLayer(Args&&... args) {
    auto layer = std::make_unique<T>(std::forward<Args>(args)...);
    T* ptr = layer.get();

    mLayers.emplace_back(std::move(layer));
    ptr->OnAttach();

    return *ptr;
  }

  template<IsLayer T, typename... Args>
  T& PushBottom(Args&&... args) {
    auto layer = std::make_unique<T>(std::forward<Args>(args)...);
    T* ptr = layer.get();

    // Vector specific: insert at begin
    mLayers.insert(mLayers.begin(), std::move(layer));
    ptr->OnAttach();

    return *ptr;
  }

  template<IsLayer T>
  T& AttachLayer(std::unique_ptr<T> layer) {
    T* ptr = layer.get();
    mLayers.emplace_back(std::move(layer));
    ptr->OnAttach();
    return *ptr;
  }
  template<IsLayer T>
  T* Get() {
    for (auto& layer : mLayers) {
      // dynamic_cast is safe here, but ensuring T is final/sealed helps compiler optimization
      if (auto p = dynamic_cast<T*>(layer.get())) {
        return p;
      }
    }
    return nullptr;
  }

  template<IsLayer T>
  std::unique_ptr<T> PopLayer() {
    auto it = std::find_if(mLayers.begin(), mLayers.end(),
                           [](const auto& l) { return dynamic_cast<T*>(l.get()) != nullptr; });

    if (it != mLayers.end()) {
      (*it)->OnDetach();

      std::unique_ptr<T> result(static_cast<T*>(it->release()));
      mLayers.erase(it);
      return result;
    }
    return nullptr;
  }

  void Clear() {
    for (auto& layer : mLayers)
      layer->OnDetach();
    mLayers.clear();
  }

  void OnSwapchainRecreated() {
    std::ranges::for_each(mLayers, [](const auto& layer) {
      if (layer->IsActive())
        layer->OnSwapchainRecreated();
    });
  }

  void OnRender(vk::CommandBuffer& cmd) const {
    std::ranges::for_each(mLayers, [&cmd](const auto& layer) {
      if (layer->IsActive())
        layer->OnRender(cmd);
    });
  }

  int OnEvent(int _cpp_par_);

private:
  std::vector<std::unique_ptr<Layer>> mLayers{};
};
} // namespace SD

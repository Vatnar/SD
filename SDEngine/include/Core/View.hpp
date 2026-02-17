#pragma once

#include <imgui.h>
#include <memory>
#include <string>
#include <vector>

#include "Core/LayerList.hpp"

namespace SD {

using ViewId = uint32_t;

enum ViewError {
  NameAlreadyExists,
  Success,
  ViewDoesNotExist,
};
class View {
public:
  explicit View(const std::string& name) : mName(name) {}
  virtual ~View() = default;

  void OnUpdate(float dt) { mLayers.Update(dt); }
  virtual void OnGuiRender() {
    if (!mOpen)
      return;
    if (ImGui::Begin(mName.c_str(), &mOpen)) {
      mLayers.GuiRender();
    }
    ImGui::End();
  }

  virtual void OnEvent(Event& e) { mLayers.OnEvent(e); }
  virtual void OnRender(vk::CommandBuffer cmd) { mLayers.OnRender(cmd); }
  virtual void OnFixedUpdate(double dt) { mLayers.OnFixedUpdate(dt); }

  template<typename T, typename... Args>
    requires std::is_base_of_v<Layer, T>
  T& PushLayer(Args&&... args) {
    return mLayers.PushLayer<T>(std::forward<Args>(args)...);
  }

  const std::string& GetName() const { return mName; }
  bool IsOpen() const { return mOpen; }
  void SetOpen(bool open) { mOpen = open; }

  LayerList& GetLayers() { return mLayers; }

private:
  std::string mName;
  LayerList mLayers;
  bool mOpen = true;
};

} // namespace SD

#pragma once

namespace sd {

struct ViewManager;
struct SceneManager;
struct LayoutManager;
struct EventManager;
struct FrameTimer;
struct LayerList;

struct ApplicationRuntime {
  ViewManager&   views;
  SceneManager&  scenes;
  LayoutManager& layout;
  EventManager&  events;
  FrameTimer&    timer;
  LayerList&     global_layers;
  bool&          hot_reload_enabled;
};

} // namespace sd

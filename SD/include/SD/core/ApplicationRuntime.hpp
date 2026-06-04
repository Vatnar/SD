#pragma once

namespace sd {

class ViewManager;
class SceneManager;
class LayoutManager;
class EventManager;
class FrameTimer;
class LayerList;

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

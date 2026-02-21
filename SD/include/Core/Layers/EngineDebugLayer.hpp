#pragma once

#include <deque>

#include "Application.hpp"
#include "Core/Layer.hpp"

namespace SD {

class Scene;
class View;
class Event;

class EngineDebugLayer : public Panel {
public:
  explicit EngineDebugLayer(Scene* scene = nullptr) : Panel("EngineDebug", scene) {}

  void OnUpdate(float dt) override;
  void OnFixedUpdate(double dt) override;
  void OnEvent(Event& e) override;
  void OnImGuiMenuBar() override;
  void OnGuiRender() override;

private:
  void DisplayViewInfo();
  void DisplaySceneSelector();
  void DisplayECSInspector();
  void DisplayEventLog();

  bool mShowViewInspector = true;
  bool mShowSceneInspector = true;
  bool mShowEventLog = true;
  bool mShowRendererInfo = true;
  bool mShowContextOverlay = false;

  bool mLogEvents = false;
  bool mLogSceneChanges = false;
  bool mLogViewResizes = false;
  bool mLogEntityLifecycle = false;

  float mTimer = 0.0f;
  int mUpdateCount = 0;
  int mFixedUpdateCount = 0;
  int mPrevFixed = 0;
  int mPrevUpdate = 0;

  Scene* mSelectedScene = nullptr;
  View* mSelectedView = nullptr;
};

} // namespace SD

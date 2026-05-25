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
  void DisplayViewInfo(View* selectedView);
  void DisplaySceneSelector();
  void DisplayECSInspector();
  void DisplayEventLog();
  void DisplayLayoutMenu();
  void DisplaySaveLayoutDialog();
  void DisplayDeleteLayoutDialog();
  void DisplayOverwriteConfirmationDialog();
  void HandleLayoutShortcuts();
  void HandleDebugShortcuts();

  // Dialog state
  bool mShowSaveLayoutDialog = false;
  bool mShowDeleteLayoutDialog = false;
  bool mShowOverwriteConfirmation = false;
  bool mSaveDialogInitialized = false;  // Track if buffer has been cleared
  std::string mPendingLayoutName;
  std::array<char, 64> mLayoutNameBuffer{};  // For save dialog input (replaces static)

  // Window visibility - LayoutManager can control these
  bool mShowViewInspector = true;
  bool mShowSceneInspector = true;
  bool mShowEventLog = true;
  bool mShowRendererInfo = true;
  bool mShowContextOverlay = false;
  
public:
  // LayoutManager interface
  void SetViewInspectorVisible(bool visible) { mShowViewInspector = visible; }
  void SetSceneInspectorVisible(bool visible) { mShowSceneInspector = visible; }
  void SetEventLogVisible(bool visible) { mShowEventLog = visible; }
  void SetRendererInfoVisible(bool visible) { mShowRendererInfo = visible; }
  void SetContextOverlayVisible(bool visible) { mShowContextOverlay = visible; }
  
  bool IsViewInspectorVisible() const { return mShowViewInspector; }
  bool IsSceneInspectorVisible() const { return mShowSceneInspector; }
  bool IsEventLogVisible() const { return mShowEventLog; }
  bool IsRendererInfoVisible() const { return mShowRendererInfo; }
  bool IsContextOverlayVisible() const { return mShowContextOverlay; }
  
  // Apply a preset configuration (for LayoutManager)
  void ApplyPresetConfiguration(bool inspectorsVisible, bool logVisible, bool rendererVisible);

  bool mLogEvents = false;
  bool mLogSceneChanges = false;
  bool mLogViewResizes = false;
  bool mLogEntityLifecycle = false;

  // Log viewer filters
  char mLogSearchBuffer[128]{};
  int mLogLevelFilter = 0;  // 0 = All, 1 = Debug+, 2 = Info+, 3 = Warn+, 4 = Error+
  bool mLogFilterInitialized = false;

  // Category tree for log filtering
  struct CategoryNode {
    std::string name;        // display name (last segment)
    std::string fullPath;    // e.g., "Engine/Renderer"
    bool visible = true;
    std::vector<CategoryNode> children;
  };
  CategoryNode mCategoryRoot{"", "", true, {}};
  bool mCategoryTreeBuilt = false;

  void BuildCategoryTree();
  void RenderCategoryNode(CategoryNode& node);
  bool IsLogVisible(const std::string& category);
  void SetCategoryVisible(CategoryNode& node, bool visible);

  float mTimer = 0.0f;
  int mUpdateCount = 0;
  int mFixedUpdateCount = 0;
  int mPrevFixed = 0;
  int mPrevUpdate = 0;

  Scene* mSelectedScene = nullptr;
  ViewId mSelectedViewId;  // Store ID instead of pointer to avoid dangling
  
  // Debug shortcut state (was static in HandleDebugShortcuts)
  bool mDebugModeActive = false;
  float mDebugModeTimer = 0.0f;
};

} // namespace SD

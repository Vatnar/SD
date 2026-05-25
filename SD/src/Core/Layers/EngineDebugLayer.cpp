#include "Core/Layers/EngineDebugLayer.hpp"

#include <array>

#include <imgui.h>

#include "Core/ECS/Components.hpp"
#include "Core/Events/window/KeyboardEvents.hpp"
#include "Core/Events/window/MouseEvents.hpp"
#include "Core/LayoutManager.hpp"
#include "Core/Logging.hpp"
#include "Core/View.hpp"
#include "Utils/Utils.hpp"

namespace SD {

void EngineDebugLayer::OnUpdate(float dt) {
  mUpdateCount++;
  mTimer += dt;

  if (mTimer >= 1.0f) {
    mPrevFixed = mFixedUpdateCount;
    mPrevUpdate = mUpdateCount;
    mFixedUpdateCount = 0;
    mUpdateCount = 0;
    mTimer = 0.0f;
  }

  // Capture resizes if logging is enabled
  auto& app = Application::Get();
  for (const auto& [id, view] : app.GetViews()) {
    if (mLogViewResizes && view->ConsumeExtentChanged()) {
      SD::Log::Engine::Debug("View '{}' resized to {}x{}", view->GetName(), view->GetExtent().width,
                      view->GetExtent().height);
    }
  }
}

void EngineDebugLayer::OnFixedUpdate(double /*dt*/) {
  mFixedUpdateCount++;
}

void EngineDebugLayer::OnEvent(Event& e) {
  if (mLogEvents) {
    std::string detail;
    switch (e.GetEventType()) {
      case EventType::KeyPressed: {
        if (auto* ke = dynamic_cast<KeyPressedEvent*>(&e)) {
          detail = fmt::format("Key: {} (mods: {}, repeat: {})", ke->key, ke->mods, ke->repeat);
        }
        break;
      }
      case EventType::KeyReleased: {
        if (auto* ke = dynamic_cast<KeyReleasedEvent*>(&e)) {
          detail = fmt::format("Key: {} (mods: {})", ke->key, ke->mods);
        }
        break;
      }
      case EventType::MousePressed: {
        if (auto* me = dynamic_cast<MousePressedEvent*>(&e)) {
          detail = fmt::format("Button: {} (mods: {}, repeat: {})", me->button, me->mods, me->repeat);
        }
        break;
      }
      case EventType::MouseReleased: {
        if (auto* me = dynamic_cast<MouseReleasedEvent*>(&e)) {
          detail = fmt::format("Button: {} (mods: {})", me->button, me->mods);
        }
        break;
      }
      case EventType::MouseMoved: {
        if (auto* me = dynamic_cast<MouseMovedEvent*>(&e)) {
          detail = fmt::format("Pos: {:.1f}, {:.1f}", me->xPos, me->yPos);
        }
        break;
      }
      case EventType::MouseScrolled: {
        if (auto* me = dynamic_cast<MouseScrolledEvent*>(&e)) {
          detail = fmt::format("Offset: {:.1f}, {:.1f}", me->xOffset, me->yOffset);
        }
        break;
      }
      default:
        detail = "Generic Event";
        break;
    }
    SD::Log::Engine::Trace("Event: {} [{}]", e.GetName(), detail);
  }
}

void EngineDebugLayer::OnImGuiMenuBar() {
  // Apply pending layout once dockspace is ready
  Application::Get().GetLayoutManager().ApplyPendingLayout();
  
  if (ImGui::BeginMenu("Debug")) {
    ImGui::MenuItem("View Inspector", nullptr, &mShowViewInspector);
    ImGui::MenuItem("Scene Inspector", nullptr, &mShowSceneInspector);
    ImGui::MenuItem("Renderer Info", nullptr, &mShowRendererInfo);
    ImGui::MenuItem("Context Overlay", nullptr, &mShowContextOverlay);
    
    ImGui::Separator();
    DisplayLayoutMenu();
    
    ImGui::Separator();
    if (ImGui::MenuItem("Reload Shaders", "Ctrl+Shift+D, R")) {
      Application::Get().ReloadShaders();
    }
    
    ImGui::EndMenu();
  }
}

void EngineDebugLayer::OnGuiRender() {
  if (mShowViewInspector) {
    if (ImGui::Begin("View Inspector", &mShowViewInspector)) {
      auto& app = Application::Get();
      const auto& views = app.GetViews();
      
      // Resolve ID to View* safely (prevents dangling pointer)
      View* selectedView = nullptr;
      if (mSelectedViewId != ViewId{}) {
        auto viewResult = app.GetView(mSelectedViewId);
        if (viewResult) {
          selectedView = &viewResult->get();
        } else {
          mSelectedViewId = ViewId{};  // View was deleted
        }
      }
      
      if (ImGui::BeginCombo("View Selector",
                            selectedView ? selectedView->GetName().c_str() : "None")) {
        for (const auto& [id, view] : views) {
          if (ImGui::Selectable(view->GetName().c_str(), mSelectedViewId == view->GetViewId())) {
            mSelectedViewId = view->GetViewId();
          }
        }
        ImGui::EndCombo();
      }
      if (selectedView)
        DisplayViewInfo(selectedView);
    }
    ImGui::End();
  }

  if (mShowSceneInspector) {
    if (ImGui::Begin("Scene Inspector", &mShowSceneInspector)) {
      DisplaySceneSelector();
      ImGui::Separator();
      DisplayECSInspector();
    }
    ImGui::End();
  }

  if (mShowEventLog) {
    if (ImGui::Begin("Engine Log", &mShowEventLog)) {
      DisplayEventLog();
    }
    ImGui::End();
  }

  if (mShowRendererInfo) {
    if (ImGui::Begin("Renderer Info", &mShowRendererInfo)) {
      ImGui::Text("App Performance: %.1f FPS", ImGui::GetIO().Framerate);
      ImGui::Text("Frame Time: %.3f ms", 1000.0f / ImGui::GetIO().Framerate);
      ImGui::Separator();
      if (ImGui::BeginTable("Timings", 2,
                            ImGuiTableFlags_Borders | ImGuiTableFlags_SizingFixedFit)) {
        ImGui::TableSetupColumn("Metric", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::Text("Fixed Updates/s");
        ImGui::TableNextColumn();
        ImGui::Text("%4d (60 Hz expected)", mPrevFixed);
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::Text("Render Frames/s");
        ImGui::TableNextColumn();
        ImGui::Text("%4d FPS", mPrevUpdate);
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::Text("Frame Work Time");
        ImGui::TableNextColumn();
        ImGui::Text("%.3f ms", Application::Get().GetFrameWorkTime() * 1000.0f);

        ImGui::EndTable();
      }

      ImGui::Separator();
      auto& app = Application::Get();
      bool autoReload = app.IsHotReloadEnabled();
      if (ImGui::Checkbox("Auto-Reload Shaders", &autoReload)) {
        app.SetHotReloadEnabled(autoReload);
      }
      if (ImGui::Button("Force Reload Shaders")) {
        app.ReloadShaders();
      }
    }
    ImGui::End();
  }

  if (mShowContextOverlay) {
    auto mousePos = ImGui::GetMousePos();
    auto& app = Application::Get();
    for (const auto& [id, view] : app.GetViews()) {
      ImVec2 regionPos = view->GetContentRegionPos();
      ImVec2 regionExtent = view->GetContentRegionExtent();
      if (mousePos.x >= regionPos.x && mousePos.x <= regionPos.x + regionExtent.x &&
          mousePos.y >= regionPos.y && mousePos.y <= regionPos.y + regionExtent.y) {
        ImGui::SetNextWindowPos(ImVec2(mousePos.x + 15, mousePos.y + 15));
        if (ImGui::Begin("##ContextOverlay", nullptr,
                         ImGuiWindowFlags_Tooltip | ImGuiWindowFlags_NoTitleBar |
                             ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_AlwaysAutoResize |
                             ImGuiWindowFlags_NoSavedSettings)) {
          float ndcX = ((mousePos.x - regionPos.x) / regionExtent.x) * 2.0f - 1.0f;
          float ndcY = ((mousePos.y - regionPos.y) / regionExtent.y) * 2.0f - 1.0f;
          ImGui::Text("View: %s", view->GetName().c_str());
          ImGui::Text("NDC: %.3f, %.3f", ndcX, ndcY);
          ImGui::End();
        }
      }
    }
  }
  
  // Display dialogs
  DisplaySaveLayoutDialog();
  DisplayDeleteLayoutDialog();
  DisplayOverwriteConfirmationDialog();
  
  // Handle shortcuts
  HandleLayoutShortcuts();
  HandleDebugShortcuts();
}

void EngineDebugLayer::DisplayViewInfo(View* selectedView) {
  if (!selectedView) return;  // Defensive null check
  
  if (ImGui::TreeNodeEx("Logging", ImGuiTreeNodeFlags_DefaultOpen)) {
    ImGui::Checkbox("Log Window Events", &mLogEvents);
    ImGui::Checkbox("Log View Resizes", &mLogViewResizes);
    ImGui::Checkbox("Show Context Overlay", &mShowContextOverlay);
    ImGui::TreePop();
  }
  ImGui::Separator();
  ImGui::Text("ID: %u | Extent: %ux%u", static_cast<uint32_t>(selectedView->GetViewId()),
              selectedView->GetExtent().width, selectedView->GetExtent().height);

  AspectMode currentMode = selectedView->GetAspectMode();
  int modeInt = (int)currentMode;
  ImGui::Text("Aspect Mode:");
  ImGui::SameLine();
  if (ImGui::RadioButton("Fixed Height", &modeInt, 0))
    selectedView->SetAspectMode(AspectMode::FixedHeight);
  ImGui::SameLine();
  if (ImGui::RadioButton("Fixed Width", &modeInt, 1))
    selectedView->SetAspectMode(AspectMode::FixedWidth);
  ImGui::SameLine();
  if (ImGui::RadioButton("Best Fit", &modeInt, 2))
    selectedView->SetAspectMode(AspectMode::BestFit);

  ImGui::Separator();
  RenderMode currentRMode = selectedView->GetRenderMode();
  const char* rModes[] = {"Shaded", "Wireframe"};
  int rModeInt = (int)currentRMode;
  if (ImGui::Combo("Render Mode", &rModeInt, rModes, IM_ARRAYSIZE(rModes))) {
    selectedView->SetRenderMode((RenderMode)rModeInt);
  }

  ImVec2 mousePos = ImGui::GetMousePos();
  ImVec2 regionPos = selectedView->GetContentRegionPos();
  ImVec2 regionExtent = selectedView->GetContentRegionExtent();

  bool isHovered = (mousePos.x >= regionPos.x && mousePos.x <= regionPos.x + regionExtent.x &&
                    mousePos.y >= regionPos.y && mousePos.y <= regionPos.y + regionExtent.y);

  ImGui::Text("Hovered: %s", isHovered ? "YES" : "NO");

  // Calculate cursor NDC relative to the specific view bounds
  float ndcX = ((mousePos.x - regionPos.x) / regionExtent.x) * 2.0f - 1.0f;
  float ndcY = ((mousePos.y - regionPos.y) / regionExtent.y) * 2.0f - 1.0f;

  ImGui::Text("Cursor NDC: %.3f, %.3f", ndcX, ndcY);

  if (ImGui::TreeNode("Matrix Data")) {
    const auto& m = selectedView->GetProjection();
    for (int i = 0; i < 4; i++) {
      ImGui::Text("%.3f %.3f %.3f %.3f", m(i, 0), m(i, 1), m(i, 2), m(i, 3));
    }
    ImGui::TreePop();
  }
}

void EngineDebugLayer::DisplaySceneSelector() {
  if (ImGui::TreeNodeEx("Logging", ImGuiTreeNodeFlags_DefaultOpen)) {
    ImGui::Checkbox("Log Scene Activity", &mLogSceneChanges);
    ImGui::Checkbox("Log Entity Lifecycle", &mLogEntityLifecycle);
    ImGui::TreePop();
  }
  ImGui::Separator();
  auto scenes = Application::Get().GetScenes();
  if (!mSelectedScene && !scenes.empty())
    mSelectedScene = scenes[0];

  std::string currentLabel = mSelectedScene ? mSelectedScene->GetName() : "None";
  if (ImGui::BeginCombo("Inspect Scene", currentLabel.c_str())) {
    for (auto* s : scenes) {
      if (ImGui::Selectable(s->GetName().c_str(), mSelectedScene == s)) {
        mSelectedScene = s;
      }
    }
    ImGui::EndCombo();
  }
}

void EngineDebugLayer::DisplayECSInspector() {
  if (!mSelectedScene) {
    ImGui::Text("No Scene selected.");
    return;
  }

  for (auto [entity, transform] : mSelectedScene->em.View<Transform>()) {
    std::string label = "Entity " + std::to_string(entity.index);
    if (auto* name = mSelectedScene->em.TryGetComponent<DebugName>(entity)) {
      label = name->name + " (ID: " + std::to_string(entity.index) + ")";
    }

    if (ImGui::TreeNode(label.c_str())) {
      if (auto* transformPtr = mSelectedScene->em.TryGetComponent<Transform>(entity)) {
        if (ImGui::TreeNode("Transform")) {
          for (int i = 0; i < 4; i++) {
            if (ImGui::DragFloat4(("row " + std::to_string(i)).c_str(),
                                  &transformPtr->worldMatrix(i, 0), 0.01f)) {
              if (mLogSceneChanges) {
                SD::Log::Engine::Debug("Entity {} transform changed", entity.index);
              }
            }
          }
          ImGui::TreePop();
        }
      }
      if (auto* renderable = mSelectedScene->em.TryGetComponent<Renderable>(entity)) {
        if (ImGui::TreeNode("Renderable")) {
          if (ImGui::ColorEdit4("Color", renderable->color)) {
            if (mLogSceneChanges) {
              SD::Log::Engine::Debug("Entity {} color changed", entity.index);
            }
          }
          if (ImGui::DragInt("Stage", (int*)&renderable->renderStage, 1, 0, 10)) {
            if (mLogSceneChanges) {
              SD::Log::Engine::Debug("Entity {} render stage changed to {}", entity.index,
                              renderable->renderStage);
            }
          }
          ImGui::TreePop();
        }
      }
      ImGui::TreePop();
    }
  }
}

void EngineDebugLayer::BuildCategoryTree() {
  auto& categories = Log::GetCategoryRegistry();
  mCategoryRoot = CategoryNode{"", "", true, {}};

  for (auto& cat : categories) {
    CategoryNode* node = &mCategoryRoot;
    size_t start = 0;
    while (start < cat.name.size()) {
      size_t end = cat.name.find('/', start);
      if (end == std::string::npos) end = cat.name.size();
      std::string segment = cat.name.substr(start, end - start);

      CategoryNode* child = nullptr;
      for (auto& c : node->children) {
        if (c.name == segment) {
          child = &c;
          break;
        }
      }
      if (!child) {
        node->children.push_back(CategoryNode{segment, cat.name.substr(0, end), cat.visible, {}});
        child = &node->children.back();
      }
      node = child;
      start = end + 1;
    }
  }
  mCategoryTreeBuilt = true;
}

void EngineDebugLayer::RenderCategoryNode(CategoryNode& node) {
  if (node.name.empty()) {
    // Root node — just render children
    for (auto& child : node.children) {
      RenderCategoryNode(child);
    }
    return;
  }

  if (node.children.empty()) {
    // Leaf node
    bool visible = node.visible;
    if (ImGui::Checkbox(node.name.c_str(), &visible)) {
      SetCategoryVisible(node, visible);
    }
  } else {
    // Parent node — TreeNode + Checkbox
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;
    bool open = ImGui::TreeNodeEx(node.name.c_str(), flags);
    ImGui::SameLine();
    bool visible = node.visible;
    if (ImGui::Checkbox("##vis", &visible)) {
      SetCategoryVisible(node, visible);
    }
    if (open) {
      for (auto& child : node.children) {
        RenderCategoryNode(child);
      }
      ImGui::TreePop();
    }
  }
}

bool EngineDebugLayer::IsLogVisible(const std::string& category) {
  // Walk the tree to find the category node
  CategoryNode* node = &mCategoryRoot;
  size_t start = 0;
  while (start < category.size()) {
    size_t end = category.find('/', start);
    if (end == std::string::npos) end = category.size();
    std::string segment = category.substr(start, end - start);

    CategoryNode* child = nullptr;
    for (auto& c : node->children) {
      if (c.name == segment) {
        child = &c;
        break;
      }
    }
    if (!child) return false;
    node = child;
    start = end + 1;
  }
  return node->visible;
}

void EngineDebugLayer::SetCategoryVisible(CategoryNode& node, bool visible) {
  node.visible = visible;
  for (auto& child : node.children) {
    SetCategoryVisible(child, visible);
  }
  // Sync back to the flat registry
  auto& categories = Log::GetCategoryRegistry();
  for (auto& cat : categories) {
    if (cat.name == node.fullPath || Log::IsCategoryUnder(cat.name, node.fullPath)) {
      cat.visible = visible;
    }
  }
}

void EngineDebugLayer::DisplayEventLog() {
  auto& history = Log::GetLogHistory();
  auto& categories = Log::GetCategoryRegistry();

  // Initialize filters once
  if (!mLogFilterInitialized) {
    mLogSearchBuffer[0] = '\0';
    mLogFilterInitialized = true;
  }

  // Search bar
  ImGui::InputText("Search", mLogSearchBuffer, sizeof(mLogSearchBuffer));
  ImGui::SameLine();

  // Level filter
  const char* levelItems[] = { "All", "Debug+", "Info+", "Warn+", "Error+" };
  ImGui::Combo("Level", &mLogLevelFilter, levelItems, IM_ARRAYSIZE(levelItems));

  // Category tree filters
  if (!mCategoryTreeBuilt || categories.size() != mCategoryRoot.children.size()) {
    BuildCategoryTree();
  }
  if (ImGui::TreeNode("Categories")) {
    for (auto& child : mCategoryRoot.children) {
      RenderCategoryNode(child);
    }
    ImGui::TreePop();
  }

  ImGui::Separator();

  // Determine minimum level to show
  Log::LogLevel minLevel = Log::LogLevel::Trace;
  switch (mLogLevelFilter) {
    case 1: minLevel = Log::LogLevel::Debug; break;
    case 2: minLevel = Log::LogLevel::Info; break;
    case 3: minLevel = Log::LogLevel::Warn; break;
    case 4: minLevel = Log::LogLevel::Error; break;
  }

  std::string search = mLogSearchBuffer;
  std::transform(search.begin(), search.end(), search.begin(), ::tolower);

  for (const auto& log : history) {
    // Level filter
    if (static_cast<int>(log.level) < static_cast<int>(minLevel))
      continue;

    // Category filter (using tree visibility)
    if (!IsLogVisible(log.category)) continue;

    // Search filter
    if (!search.empty()) {
      std::string msgLower = log.message;
      std::transform(msgLower.begin(), msgLower.end(), msgLower.begin(), ::tolower);
      if (msgLower.find(search) == std::string::npos) continue;
    }

    // Category color
    ImVec4 catColor = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
    for (auto& cat : categories) {
      if (cat.name == log.category) {
        catColor = cat.color;
        break;
      }
    }

    // Level color for the tag
    ImVec4 levelColor = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
    switch (log.level) {
      case Log::LogLevel::Trace:    levelColor = ImVec4(0.7f, 0.7f, 0.7f, 1.0f); break;
      case Log::LogLevel::Debug:    levelColor = ImVec4(0.0f, 0.8f, 1.0f, 1.0f); break;
      case Log::LogLevel::Info:     levelColor = ImVec4(0.0f, 1.0f, 0.0f, 1.0f); break;
      case Log::LogLevel::Warn:     levelColor = ImVec4(1.0f, 1.0f, 0.0f, 1.0f); break;
      case Log::LogLevel::Error:    levelColor = ImVec4(1.0f, 0.0f, 0.0f, 1.0f); break;
      case Log::LogLevel::Critical: levelColor = ImVec4(1.0f, 0.0f, 1.0f, 1.0f); break;
      default: break;
    }

    // Format: [+sec] [Category] [level] message
    ImGui::Text("[+%.3fs] ", log.uptimeSec);
    ImGui::SameLine();
    ImGui::TextColored(catColor, "[%s]", log.category.c_str());
    ImGui::SameLine();

    const char* levelStr = "?";
    switch (log.level) {
      case Log::LogLevel::Trace:    levelStr = "trace"; break;
      case Log::LogLevel::Debug:    levelStr = "debug"; break;
      case Log::LogLevel::Info:     levelStr = "info"; break;
      case Log::LogLevel::Warn:     levelStr = "warn"; break;
      case Log::LogLevel::Error:    levelStr = "error"; break;
      case Log::LogLevel::Critical: levelStr = "critical"; break;
      default: break;
    }
    ImGui::TextColored(levelColor, "[%s]", levelStr);
    ImGui::SameLine();
    ImGui::Text("%s", log.message.c_str());
  }

  if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
    ImGui::SetScrollHereY(1.0f);
}

void EngineDebugLayer::DisplayLayoutMenu() {
  if (ImGui::BeginMenu("Layout")) {
    auto& layoutManager = Application::Get().GetLayoutManager();
    const std::string& currentLayout = layoutManager.GetCurrentLayout();
    
    // Presets
    bool isMinimal = (currentLayout == "Minimal");
    if (ImGui::MenuItem("Minimal", "Alt+1", isMinimal)) {
      layoutManager.ApplyPreset(LayoutManager::Preset::Minimal);
    }
    
    bool isDefault = (currentLayout == "Default");
    if (ImGui::MenuItem("Default", "Alt+2", isDefault)) {
      layoutManager.ApplyPreset(LayoutManager::Preset::Default);
    }
    
    // Custom layouts
    auto layouts = layoutManager.GetUserLayoutNames();
    if (!layouts.empty()) {
      ImGui::Separator();
      int shortcutNum = 3;
      for (const auto& name : layouts) {
        bool isCurrent = (name == currentLayout);
        std::string shortcut = (shortcutNum <= 9) ? "Alt+" + std::to_string(shortcutNum) : "";
        if (ImGui::MenuItem(name.c_str(), shortcut.c_str(), isCurrent)) {
          layoutManager.LoadLayout(name);
        }
        shortcutNum++;
      }
    }
    
    ImGui::Separator();
    
    // Save and Delete options
    if (ImGui::MenuItem("Save Layout...", "Alt+0")) {
      mShowSaveLayoutDialog = true;
    }
    if (ImGui::MenuItem("Delete Layout...")) {
      mShowDeleteLayoutDialog = true;
    }
    
    ImGui::EndMenu();
  }
}

void EngineDebugLayer::DisplaySaveLayoutDialog() {
  if (!mShowSaveLayoutDialog) {
    mSaveDialogInitialized = false;
    return;
  }
  
  // Clear buffer only once when dialog first opens
  if (!mSaveDialogInitialized) {
    mLayoutNameBuffer.fill('\0');
    mSaveDialogInitialized = true;
  }
  ImGui::OpenPopup("Save Layout");
  
  if (ImGui::BeginPopupModal("Save Layout", &mShowSaveLayoutDialog)) {
    ImGui::InputText("Layout Name", mLayoutNameBuffer.data(), mLayoutNameBuffer.size());
    
    ImGui::Spacing();
    
    bool saveClicked = ImGui::Button("Save");
    ImGui::SameLine();
    if (ImGui::Button("Cancel")) {
      mShowSaveLayoutDialog = false;
    }
    
    if (saveClicked && mLayoutNameBuffer[0] != '\0') {
      auto& layoutManager = Application::Get().GetLayoutManager();
      std::string layoutName(mLayoutNameBuffer.data());
      
      if (layoutManager.HasLayout(layoutName)) {
        // Show overwrite confirmation
        mPendingLayoutName = layoutName;
        mShowOverwriteConfirmation = true;
        mShowSaveLayoutDialog = false;
      } else {
        layoutManager.SaveLayout(layoutName);
        mShowSaveLayoutDialog = false;
      }
    }
    
    ImGui::EndPopup();
  }
}

void EngineDebugLayer::DisplayDeleteLayoutDialog() {
  if (!mShowDeleteLayoutDialog) return;
  
  ImGui::OpenPopup("Delete Layout");
  
  if (ImGui::BeginPopupModal("Delete Layout", &mShowDeleteLayoutDialog)) {
    auto& layoutManager = Application::Get().GetLayoutManager();
    auto layouts = layoutManager.GetUserLayoutNames();
    
    ImGui::Text("Select layout to delete:");
    ImGui::Spacing();
    
    for (const auto& name : layouts) {
      ImGui::PushID(name.c_str());
      if (ImGui::Button("Delete", ImVec2(200, 0))) {
        layoutManager.DeleteLayout(name);
      }
      ImGui::SameLine();
      ImGui::Text("\"%s\"", name.c_str());
      ImGui::PopID();
    }
    
    if (layouts.empty()) {
      ImGui::TextDisabled("No custom layouts to delete");
    }
    
    ImGui::Spacing();
    if (ImGui::Button("Close")) {
      mShowDeleteLayoutDialog = false;
    }
    
    ImGui::EndPopup();
  }
}

void EngineDebugLayer::DisplayOverwriteConfirmationDialog() {
  if (!mShowOverwriteConfirmation) return;
  
  ImGui::OpenPopup("Confirm Overwrite");
  
  if (ImGui::BeginPopupModal("Confirm Overwrite", &mShowOverwriteConfirmation)) {
    ImGui::Text("Layout \"%s\" already exists.", mPendingLayoutName.c_str());
    ImGui::Text("Do you want to overwrite it?");
    ImGui::Spacing();
    
    if (ImGui::Button("Yes")) {
      auto& layoutManager = Application::Get().GetLayoutManager();
      layoutManager.SaveLayout(mPendingLayoutName);
      mShowOverwriteConfirmation = false;
      mPendingLayoutName.clear();
    }
    
    ImGui::SameLine();
    if (ImGui::Button("No")) {
      mShowOverwriteConfirmation = false;
      mPendingLayoutName.clear();
    }
    
    ImGui::EndPopup();
  }
}

void EngineDebugLayer::HandleLayoutShortcuts() {
  // Alt+Number shortcuts for layouts
  ImGuiIO& io = ImGui::GetIO();
  
  if (io.KeyAlt && !io.KeyCtrl && !io.KeyShift) {
    auto& layoutManager = Application::Get().GetLayoutManager();
    
    // Check each number key individually
    if (ImGui::IsKeyPressed(ImGuiKey_1)) {
      layoutManager.ApplyPreset(LayoutManager::Preset::Minimal);
    } else if (ImGui::IsKeyPressed(ImGuiKey_2)) {
      layoutManager.ApplyPreset(LayoutManager::Preset::Default);
    } else if (ImGui::IsKeyPressed(ImGuiKey_0)) {
      mShowSaveLayoutDialog = true;
    } else {
      // Alt+3 through Alt+9 for custom layouts
      int customIndex = -1;
      if (ImGui::IsKeyPressed(ImGuiKey_3)) customIndex = 0;
      else if (ImGui::IsKeyPressed(ImGuiKey_4)) customIndex = 1;
      else if (ImGui::IsKeyPressed(ImGuiKey_5)) customIndex = 2;
      else if (ImGui::IsKeyPressed(ImGuiKey_6)) customIndex = 3;
      else if (ImGui::IsKeyPressed(ImGuiKey_7)) customIndex = 4;
      else if (ImGui::IsKeyPressed(ImGuiKey_8)) customIndex = 5;
      else if (ImGui::IsKeyPressed(ImGuiKey_9)) customIndex = 6;
      
      if (customIndex >= 0) {
        auto layouts = layoutManager.GetUserLayoutNames();
        if (customIndex < static_cast<int>(layouts.size())) {
          layoutManager.LoadLayout(layouts[customIndex]);
        }
      }
    }
  }
}

void EngineDebugLayer::HandleDebugShortcuts() {
  // Ctrl+Shift+D prefix for debug shortcuts
  // Uses member variables mDebugModeActive and mDebugModeTimer (not static, for thread safety)
  
  ImGuiIO& io = ImGui::GetIO();
  
  // Check for Ctrl+Shift+D
  if (io.KeyCtrl && io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_D)) {
    mDebugModeActive = true;
    mDebugModeTimer = 0.5f; // 500ms timeout
  }
  
  if (mDebugModeActive) {
    // Decrement timer
    mDebugModeTimer -= io.DeltaTime;
    
    // Check for second key
    if (ImGui::IsKeyPressed(ImGuiKey_R)) {
      Application::Get().ReloadShaders();
      mDebugModeActive = false;
    } else if (ImGui::IsKeyPressed(ImGuiKey_H)) {
      auto& app = Application::Get();
      app.SetHotReloadEnabled(!app.IsHotReloadEnabled());
      mDebugModeActive = false;
    } else if (ImGui::IsKeyPressed(ImGuiKey_L)) {
      mShowContextOverlay = !mShowContextOverlay;
      mDebugModeActive = false;
    } else if (mDebugModeTimer <= 0.0f) {
      // Timer expired - open debug wheel (placeholder)
      SD::Log::Engine::Info("Debug wheel placeholder - coming soon!");
      mDebugModeActive = false;
    }
  }
}

void EngineDebugLayer::ApplyPresetConfiguration(bool inspectorsVisible, bool logVisible,
                                                bool rendererVisible) {
  mShowViewInspector = inspectorsVisible;
  mShowSceneInspector = inspectorsVisible;
  mShowEventLog = logVisible;
  mShowRendererInfo = rendererVisible;
}

} // namespace SD

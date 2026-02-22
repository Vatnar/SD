#include "Core/Layers/EngineDebugLayer.hpp"

#include <imgui.h>
#include <spdlog/fmt/fmt.h>
#include <spdlog/spdlog.h>

#include "Core/ECS/Components.hpp"
#include "Core/Events/window/KeyboardEvents.hpp"
#include "Core/Events/window/MouseEvents.hpp"
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
      if (auto logger = spdlog::get("engine")) {
        logger->debug("View '{}' resized to {}x{}", view->GetName(), view->GetExtent().width,
                      view->GetExtent().height);
      }
    }
  }
}

void EngineDebugLayer::OnFixedUpdate(double dt) {
  (void)dt;
  mFixedUpdateCount++;
}

void EngineDebugLayer::OnEvent(Event& e) {
  if (mLogEvents) {
    if (const auto logger = spdlog::get("engine")) {
      std::string detail;
      switch (e.GetEventType()) {
        case EventType::KeyPressed: {
          auto& ke = static_cast<KeyPressedEvent&>(e);
          detail = fmt::format("Key: {} (mods: {}, repeat: {})", ke.key, ke.mods, ke.repeat);
          break;
        }
        case EventType::KeyReleased: {
          auto& ke = static_cast<KeyReleasedEvent&>(e);
          detail = fmt::format("Key: {} (mods: {})", ke.key, ke.mods);
          break;
        }
        case EventType::MousePressed: {
          auto& me = static_cast<MousePressedEvent&>(e);
          detail = fmt::format("Button: {} (mods: {}, repeat: {})", me.button, me.mods, me.repeat);
          break;
        }
        case EventType::MouseReleased: {
          auto& me = static_cast<MouseReleasedEvent&>(e);
          detail = fmt::format("Button: {} (mods: {})", me.button, me.mods);
          break;
        }
        case EventType::MouseMoved: {
          auto& me = static_cast<MouseMovedEvent&>(e);
          detail = fmt::format("Pos: {:.1f}, {:.1f}", me.xPos, me.yPos);
          break;
        }
        case EventType::MouseScrolled: {
          auto& me = static_cast<MouseScrolledEvent&>(e);
          detail = fmt::format("Offset: {:.1f}, {:.1f}", me.xOffset, me.yOffset);
          break;
        }
        default:
          detail = "Generic Event";
          break;
      }
      logger->trace("Event: {} [{}]", e.GetName(), detail);
    }
  }
}

void EngineDebugLayer::OnImGuiMenuBar() {
  if (ImGui::BeginMenu("Debug")) {
    ImGui::MenuItem("View Inspector", nullptr, &mShowViewInspector);
    ImGui::MenuItem("Scene Inspector", nullptr, &mShowSceneInspector);
    ImGui::MenuItem("Renderer Info", nullptr, &mShowRendererInfo);
    ImGui::MenuItem("Context Overlay", nullptr, &mShowContextOverlay);
    ImGui::EndMenu();
  }
}

void EngineDebugLayer::OnGuiRender() {
  if (mShowViewInspector) {
    if (ImGui::Begin("View Inspector", &mShowViewInspector)) {
      auto& app = Application::Get();
      const auto& views = app.GetViews();
      if (ImGui::BeginCombo("View Selector",
                            mSelectedView ? mSelectedView->GetName().c_str() : "None")) {
        for (const auto& [id, view] : views) {
          if (ImGui::Selectable(view->GetName().c_str(), mSelectedView == view.get())) {
            mSelectedView = view.get();
          }
        }
        ImGui::EndCombo();
      }
      if (mSelectedView)
        DisplayViewInfo();
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
}

void EngineDebugLayer::DisplayViewInfo() {
  if (ImGui::TreeNodeEx("Logging", ImGuiTreeNodeFlags_DefaultOpen)) {
    ImGui::Checkbox("Log Window Events", &mLogEvents);
    ImGui::Checkbox("Log View Resizes", &mLogViewResizes);
    ImGui::Checkbox("Show Context Overlay", &mShowContextOverlay);
    ImGui::TreePop();
  }
  ImGui::Separator();
  ImGui::Text("ID: %u | Extent: %ux%u", mSelectedView->ViewId(), mSelectedView->GetExtent().width,
              mSelectedView->GetExtent().height);

  AspectMode currentMode = mSelectedView->GetAspectMode();
  int modeInt = (int)currentMode;
  ImGui::Text("Aspect Mode:");
  ImGui::SameLine();
  if (ImGui::RadioButton("Fixed Height", &modeInt, 0))
    mSelectedView->SetAspectMode(AspectMode::FixedHeight);
  ImGui::SameLine();
  if (ImGui::RadioButton("Fixed Width", &modeInt, 1))
    mSelectedView->SetAspectMode(AspectMode::FixedWidth);
  ImGui::SameLine();
  if (ImGui::RadioButton("Best Fit", &modeInt, 2))
    mSelectedView->SetAspectMode(AspectMode::BestFit);

  ImGui::Separator();
  RenderMode currentRMode = mSelectedView->GetRenderMode();
  const char* rModes[] = {"Shaded", "Wireframe"};
  int rModeInt = (int)currentRMode;
  if (ImGui::Combo("Render Mode", &rModeInt, rModes, IM_ARRAYSIZE(rModes))) {
    mSelectedView->SetRenderMode((RenderMode)rModeInt);
  }

  ImVec2 mousePos = ImGui::GetMousePos();
  ImVec2 regionPos = mSelectedView->GetContentRegionPos();
  ImVec2 regionExtent = mSelectedView->GetContentRegionExtent();

  bool isHovered = (mousePos.x >= regionPos.x && mousePos.x <= regionPos.x + regionExtent.x &&
                    mousePos.y >= regionPos.y && mousePos.y <= regionPos.y + regionExtent.y);

  ImGui::Text("Hovered: %s", isHovered ? "YES" : "NO");

  // Calculate cursor NDC relative to the specific view bounds
  float ndcX = ((mousePos.x - regionPos.x) / regionExtent.x) * 2.0f - 1.0f;
  float ndcY = ((mousePos.y - regionPos.y) / regionExtent.y) * 2.0f - 1.0f;

  ImGui::Text("Cursor NDC: %.3f, %.3f", ndcX, ndcY);

  if (ImGui::TreeNode("Matrix Data")) {
    const auto& m = mSelectedView->GetProjection();
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

  SD_ASSERT(mSelectedScene, "Selected scene must be valid");

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
                if (auto logger = spdlog::get("engine"))
                  logger->debug("Entity {} transform changed", entity.index);
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
              if (auto logger = spdlog::get("engine"))
                logger->debug("Entity {} color changed", entity.index);
            }
          }
          if (ImGui::DragInt("Stage", (int*)&renderable->renderStage, 1, 0, 10)) {
            if (mLogSceneChanges) {
              if (auto logger = spdlog::get("engine"))
                logger->debug("Entity {} render stage changed to {}", entity.index,
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

void EngineDebugLayer::DisplayEventLog() {
  auto& history = Log::GetLogHistory();
  for (const auto& log : history) {
    ImVec4 color = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
    switch (log.level) {
      case Log::LogLevel::Trace:
        color = ImVec4(0.7f, 0.7f, 0.7f, 1.0f);
        break;
      case Log::LogLevel::Debug:
        color = ImVec4(0.0f, 0.8f, 1.0f, 1.0f);
        break;
      case Log::LogLevel::Info:
        color = ImVec4(0.0f, 1.0f, 0.0f, 1.0f);
        break;
      case Log::LogLevel::Warn:
        color = ImVec4(1.0f, 1.0f, 0.0f, 1.0f);
        break;
      case Log::LogLevel::Error:
        color = ImVec4(1.0f, 0.0f, 0.0f, 1.0f);
        break;
      case Log::LogLevel::Critical:
        color = ImVec4(1.0f, 0.0f, 1.0f, 1.0f);
        break;
      default:
        break;
    }
    ImGui::TextColored(color, "%s", log.message.c_str());
  }
  if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
    ImGui::SetScrollHereY(1.0f);
}

} // namespace SD

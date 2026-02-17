#include "Application.hpp"
#include "Core/SceneView.hpp"
#include "Entrypoint.hpp"


class UpdateTimingsLayer : public SD::Layer {
public:
  UpdateTimingsLayer() : Layer("UpdateTimings"), mPrevFixed(0), mPrevUpdate(0) {}


  void OnUpdate(float dt) override {
    mUpdateCount++;
    mTimer += dt;

    if (mTimer >= 1.0f) {
      mPrevFixed = mFixedUpdateCount;
      mPrevUpdate = mUpdateCount;
      mFixedUpdateCount = 0;
      mUpdateCount = 0;
      mTimer = 0.0f;
    }
  }

  void OnFixedUpdate(double fixedDelta) override {
    mFixedUpdateCount++;

    mPhysicsTime += fixedDelta;
  }
  void OnGuiRender() override {
    ImGui::Begin("Timings");
    if (ImGui::BeginTable("Timings", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_SizingFixedFit)) {
      ImGui::TableSetupColumn("Metric", ImGuiTableColumnFlags_WidthFixed);
      ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Text("Fixed Updates");
      ImGui::TableNextColumn();
      ImGui::Text("%4d (60 Hz)", mPrevFixed);
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Text("Frame Rate");
      ImGui::TableNextColumn();
      ImGui::Text("%4d FPS", mPrevUpdate);

      ImGui::EndTable();
    }
    ImGui::End();
  }

private:
  float mTimer = 0.0f;
  int mUpdateCount = 0;
  int mFixedUpdateCount = 0;

  int mPrevFixed;
  int mPrevUpdate;

  double mPhysicsTime = 0.0;
};


class GameLayer : public SD::Layer {
public:
  using Layer::Layer;
  void OnGuiRender() override {
    ImGui::Text("--- Game View ---");
    if (mScene) {
      ImGui::Text("Scene connected.");
      if (ImGui::Button("Spawn Entity")) {
        mScene->GetEntityManager().Create();
      }
    }
  }
};

class DebugLayer : public SD::Layer {
public:
  using Layer::Layer;
  void OnGuiRender() override {
    ImGui::Text("--- Debug View ---");
    if (mScene) {
      auto& em = mScene->GetEntityManager();
      ImGui::Text("Monitoring shared Scene...");
      ImGui::Separator();
      ImGui::Text("Count: %d", em.GetEntityCount());
    }
  }
};
class RectangleLayer : public SD::Layer {
public:
  using Layer::Layer;
  void OnRender(vk::CommandBuffer cmd) override {}
  void OnGuiRender() override {
    ImGui::Text("Rectangle Viewport (Vulkan Drawing)");
    ImGui::Text("Size: %.0f x %.0f", ImGui::GetContentRegionAvail().x,
                ImGui::GetContentRegionAvail().y);

    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImVec2 p0 = ImGui::GetCursorScreenPos();
    ImVec2 p1 = ImVec2(p0.x + 100, p0.y + 100);
    draw_list->AddRectFilled(p0, p1, IM_COL32(255, 0, 0, 255));
    ImGui::TextColored(ImVec4(1, 0, 0, 1), "Vulkan-backed Rect");
  }
};

class GuiTest : public SD::Layer {
public:
  explicit GuiTest(SD::Scene* anotherScene) :
    Layer("Sandbox Controls"), mAnotherScene(anotherScene) {}

  void OnGuiRender() override {
    ImGui::Begin("Workspace Manager");
    if (ImGui::Button("Toggle Game View")) {
      auto& app = SD::Application::Get();
      if (auto id = app.GetViewId("Game View"))
        app.RemoveView(id.value());
      else {
        auto view = app.CreateView<SD::View>("Game View").value();
        app.PushLayerToView<GameLayer>(view, "GameLogic", mScene);
      }
    }
    if (ImGui::Button("Toggle Game2 View")) {
      auto& app = SD::Application::Get();
      if (app.GetView("Game2 View"))
        app.RemoveView("Game2 View");
      else {
        auto view = app.CreateView<SD::View>("Game2 View").value();
        app.PushLayerToView<GameLayer>(view, "GameLogic", mAnotherScene);
      }
    }
    if (ImGui::Button("Toggle Debug View")) {
      auto& app = SD::Application::Get();
      if (app.GetView("Debug View")) {
        app.RemoveView("Debug View");
      } else {
        auto view = app.CreateView<SD::View>("Debug View").value();
        app.PushLayerToView<DebugLayer>(view, "Debug View", mScene);
      }
    }
    if (ImGui::Button("Toggle Debug2 View")) {
      auto& app = SD::Application::Get();
      if (app.GetView("Debug2 View"))
        app.RemoveView("Debug2 View");
      else {
        auto view = app.CreateView<SD::View>("Debug2 View").value();
        app.PushLayerToView<DebugLayer>(view, "Debug view", mAnotherScene);
      }
    }
    ImGui::End();
  }

private:
  SD::Scene* mAnotherScene;
  int mSceneCount = 0;
};

class SandboxApp : public SD::Application {
public:
  SandboxApp() :
    Application({"Sandbox", 1280, 720}), mSharedScene(std::make_unique<SD::Scene>()),
    mAnotherScene(std::make_unique<SD::Scene>()) {
    PushGlobalLayer<UpdateTimingsLayer>();

    SD::WindowId mainWin = 0;
    auto& ctrl = PushViewLayer<GuiTest>(mainWin, mAnotherScene.get());
    ctrl.SetScene(mSharedScene.get());
  }

private:
  std::unique_ptr<SD::Scene> mSharedScene;
  std::unique_ptr<SD::Scene> mAnotherScene;
};

SD::Application* CreateApplication() {
  return new SandboxApp();
}

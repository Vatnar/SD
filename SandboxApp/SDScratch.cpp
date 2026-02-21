#include "Application.hpp"
#include "Core/ECS/Components.hpp"
#include "Core/Layers/EngineDebugLayer.hpp"
#include "Core/SceneView.hpp"
#include "GameRenderLayer.hpp"
#include "RuntimeStateManager.hpp"

#define SDSCRATCH_API extern "C" __declspec(dllexport)

// Render stage: handles GPU command recording for the game world


extern void Terminate();

class GuiTest : public SD::Panel {
public:
  explicit GuiTest(SD::Scene* anotherScene) :
    Panel("Sandbox Controls"), mAnotherScene(anotherScene) {}


  void OnGuiRender() override {}

private:
  SD::Scene* mAnotherScene;
  int mSceneCount = 0;
};

class SandboxApp : public SD::Application {
public:
  SandboxApp(SD::RuntimeStateManager* stateManager = nullptr) :
    Application({"Sandbox", 1280, 720}, stateManager) {
    // TOOD: Create scenes, scenemanager

    mSharedScene = stateManager->CreateScene("MainScene");
    mAnotherScene = stateManager->CreateScene("AnotherScene");
    // TODO: only init if not already existing


    SD::WindowId mainWin = 0;
    auto& ctrl = PushViewLayer<GuiTest>(mainWin, mAnotherScene);
    ctrl.SetScene(mSharedScene);
    auto& app = Get();
    auto& gameView = app.CreateView<SD::View>("Game");
    auto& renderer = app.GetRenderer();
    auto& pipelines = renderer.GetPipelineFactory();
    gameView.SetupLayeredRender(3);

    // Create push constant layout for MVP + colour
    struct Push {
      VLA::Matrix4x4f mvp{};
      float color[4]{};
    };
    VkPipelineLayout pushLayout =
        pipelines.CreatePushConstantLayout(VK_SHADER_STAGE_VERTEX_BIT, sizeof(Push));

    SD::PipelineFactory::PipelineDesc worldDesc{.vertPath = "assets/engine/shaders/world.vert",
                                                .fragPath = "assets/engine/shaders/world.frag",
                                                .renderPass = gameView.GetLayeredRenderPass(),
                                                .subpass = 1};
    VkRenderPass pass = gameView.GetLayeredRenderPass();
    SD_CORE_ASSERT(pass, "Null renderpass");

    VkPipeline worldPipe = pipelines.CreateGraphicsPipeline(worldDesc, pushLayout);
    SD_CORE_ASSERT(worldPipe, "Pipeline creation failed");

    SD::PipelineFactory::PipelineDesc wireDesc = worldDesc;
    wireDesc.fragPath = worldDesc.fragPath;
    wireDesc.vertPath = worldDesc.vertPath;

    wireDesc.polygonMode = VK_POLYGON_MODE_LINE;
    VkPipeline wirePipe = pipelines.CreateGraphicsPipeline(wireDesc, pushLayout);

    // todo: fix wireframe
    gameView.PushLayer<GameRenderLayer>(1, "GameRender", mSharedScene, worldPipe, worldPipe,
                                        pushLayout);
    app.PushGlobalLayer<SD::EngineDebugLayer>(mSharedScene);

    auto ent = mSharedScene->em.Create();
    mSharedScene->em.AddComponent<SD::DebugName>(ent, "Triangle");
    mSharedScene->em.AddComponent<SD::Transform>(ent, VLA::Matrix4x4f::Identity);
    mSharedScene->em.AddComponent<SD::Renderable>(ent, SD::Renderable{0, 0, 1, 1});

    auto ent2 = mSharedScene->em.Create();
    auto mat2 = VLA::Matrix4x4f::Identity;

    mat2.Column(3) = {3, 0, 0, 1};
    mSharedScene->em.AddComponent<SD::DebugName>(ent2, "Better triangle");
    mSharedScene->em.AddComponent<SD::Transform>(ent2, VLA::Matrix4x4f::Identity);
    mSharedScene->em.AddComponent<SD::Renderable>(ent2, SD::Renderable{0, 0, 1, 1});
  }

private:
  SD::Scene* mSharedScene;
  SD::Scene* mAnotherScene;
};

// API for engine
SDSCRATCH_API SD::Application* CreateApplication(SD::RuntimeStateManager* runtimeManager) {
  return new SandboxApp(runtimeManager);
}
SDSCRATCH_API void PluginStop() {
  // stopFlag.store(true);
}

void SetScene(SD::Scene* scene) {
  // SandboxApp::Get().SetHotReloadEnabled(true);
}

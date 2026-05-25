#include <iostream>
#include <memory>

#include "Application.hpp"
#include "Core/ECS/Components.hpp"
#include "Core/Layers/EngineDebugLayer.hpp"
#include "Core/Vulkan/VulkanRenderer.hpp"
#include "GameContext.hpp"
#include "GameRenderLayer.hpp"
#include "Logging.hpp"
#include "RuntimeStateManager.hpp"

static void RegisterGameCategories() {
  SD::Log::RegisterCategory("Game", ImVec4(0.4f, 1.0f, 0.4f, 1.0f));
  SD::Log::RegisterCategory("Game/Combat", ImVec4(1.0f, 0.6f, 0.0f, 1.0f));
  SD::Log::RegisterCategory("Game/UI", ImVec4(1.0f, 0.4f, 0.8f, 1.0f));
  SD::Log::RegisterCategory("Game/Audio", ImVec4(0.4f, 0.8f, 1.0f, 1.0f));
}

class SandboxGame : public SD::GameContext {
public:
  explicit SandboxGame(SD::RuntimeStateManager* stateManager) : mStateManager(stateManager) {}

  void OnLoad(SD::Application& app) override {
    mSharedScene = app.CreateScene("MainScene");
    mAnotherScene = app.CreateScene("AnotherScene");

    auto& gameView = app.CreateView<SD::View>("Game");
    auto& renderer = app.GetRenderer();
    auto& pipelines = renderer.GetPipelineFactory();
    gameView.SetupLayeredRender(3);

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

    auto worldPipeHandle = pipelines.CreateGraphicsPipeline(worldDesc, pushLayout);
    SD_CORE_ASSERT(worldPipeHandle, "Pipeline creation failed");

    SD::PipelineFactory::PipelineDesc wireDesc = worldDesc;
    wireDesc.polygonMode = VK_POLYGON_MODE_LINE;
    auto wirePipeHandle = pipelines.CreateGraphicsPipeline(wireDesc, pushLayout);

    gameView.PushLayer<GameRenderLayer>(1, "GameRender", mSharedScene, worldPipeHandle,
                                        wirePipeHandle, pushLayout, &pipelines);
    app.PushGlobalLayer<SD::EngineDebugLayer>(mSharedScene);

    auto ent = mSharedScene->em.Create();
    mSharedScene->em.AddComponent<SD::DebugName>(ent, "Triangle");
    mSharedScene->em.AddComponent<SD::Transform>(ent, VLA::Matrix4x4f::Identity());
    mSharedScene->em.AddComponent<SD::Renderable>(ent, SD::Renderable{0, 0, 1, 1});

    auto ent2 = mSharedScene->em.Create();
    auto mat2 = VLA::Matrix4x4f::Identity();

    mat2.Column(3) = {3, 0, 0, 1};
    mSharedScene->em.AddComponent<SD::DebugName>(ent2, "Better triangle");
    mSharedScene->em.AddComponent<SD::Transform>(ent2, VLA::Matrix4x4f::Identity());
    mSharedScene->em.AddComponent<SD::Renderable>(ent2, SD::Renderable{0, 0, 1, 1});

    if (mStateManager && mStateManager->HasState()) {
      mStateManager->Restore(&app);
    }
  }

  void OnUpdate(float dt) override { (void)dt; }

  void OnUnload() override {
    mSharedScene = nullptr;
    mAnotherScene = nullptr;
  }

  std::vector<SD::Scene*> GetScenes() override {
    std::vector<SD::Scene*> scenes;
    if (mSharedScene)
      scenes.push_back(mSharedScene);
    if (mAnotherScene && mAnotherScene != mSharedScene)
      scenes.push_back(mAnotherScene);
    return scenes;
  }

private:
  SD::RuntimeStateManager* mStateManager;
  SD::Scene* mSharedScene = nullptr;
  SD::Scene* mAnotherScene = nullptr;
};

int main() {
  SD::Log::Init();

  SD::Log::Game::Info("Starting game");

  std::unique_ptr<SD::RuntimeStateManager> stateManager;
  stateManager = std::make_unique<SD::RuntimeStateManager>();

  SD::ApplicationSpecification spec;
  spec.name = "Sandbox";
  spec.width = 1280;
  spec.height = 720;
  spec.enableHotReload = false;

  std::unique_ptr<SD::Application> app;
  app = std::make_unique<SD::Application>(spec, stateManager.get());

  stateManager->SetApplication(app.get());

  std::unique_ptr<SD::GameContext> game;
  game = std::make_unique<SandboxGame>(stateManager.get());

  app->SetGameContext(game.get());

  RegisterGameCategories();
  game->OnLoad(*app);

  std::atomic stopFlag{false};
  app->Run(&stopFlag);

  return 0;
}

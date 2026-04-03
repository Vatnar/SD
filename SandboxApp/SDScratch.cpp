#include <cstdio>

#include "Application.hpp"
#include "Core/ECS/Components.hpp"
#include "Core/Layers/EngineDebugLayer.hpp"
#include "Core/SceneView.hpp"
#include "GameRenderLayer.hpp"
#include "game.h"

// Wrap SD types for C interface
static SD::Application* ToApp(SD_Application* app) {
  return reinterpret_cast<SD::Application*>(app);
}
static SD::Scene* ToScene(SD_Scene* scene) {
  return reinterpret_cast<SD::Scene*>(scene);
}
[[maybe_unused]] static SD_Application* FromApp(SD::Application* app) {
  return reinterpret_cast<SD_Application*>(app);
}
static SD_Scene* FromScene(SD::Scene* scene) {
  return reinterpret_cast<SD_Scene*>(scene);
}

static void GameOnLoad(SD_Application* appPtr, GameState* state) {
  SD::Application& app = *ToApp(appPtr);

  state->version++;
  printf("GAME VERSION %d LOADED\n", state->version);

  state->sharedScene = FromScene(app.CreateScene("MainScene"));
  state->anotherScene = FromScene(app.CreateScene("AnotherScene"));


  SD::WindowId mainWin = 0;
  auto& ctrl =
      app.PushViewLayer<SD::Panel>(mainWin, "Sandbox Controls", ToScene(state->anotherScene));
  ctrl.SetScene(ToScene(state->sharedScene));

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

  VkPipeline worldPipe = pipelines.CreateGraphicsPipeline(worldDesc, pushLayout);

  SD::PipelineFactory::PipelineDesc wireDesc = worldDesc;
  wireDesc.polygonMode = VK_POLYGON_MODE_LINE;
  // VkPipeline wirePipe = pipelines.CreateGraphicsPipeline(wireDesc, pushLayout);

  SPDLOG_INFO("Pushing GameRenderLayer");
  gameView.PushLayer<GameRenderLayer>(1, "GameRender", ToScene(state->sharedScene), worldPipe,
                                      worldPipe, pushLayout);
  app.PushGlobalLayer<SD::EngineDebugLayer>(ToScene(state->sharedScene));

  auto scene = ToScene(state->sharedScene);
  auto ent = scene->em.Create();
  scene->em.AddComponent<SD::DebugName>(ent, "Triangle");
  scene->em.AddComponent<SD::Transform>(ent, VLA::Matrix4x4f::Identity);
  scene->em.AddComponent<SD::Renderable>(ent, SD::Renderable{0, 0, 1, 1});


  auto ent2 = scene->em.Create();
  scene->em.AddComponent<SD::DebugName>(ent2, "Better triangle");
  scene->em.AddComponent<SD::Transform>(ent2, VLA::Matrix4x4f::Identity);
  scene->em.AddComponent<SD::Renderable>(ent2, SD::Renderable{0, 0, 1, 1});

  spdlog::get("engine")->info("Game loaded, version {}", state->version);
}

static void GameOnUpdate(SD_Application* appPtr, GameState* state, float dt) {
  (void)appPtr;
  (void)state;
  (void)dt;
}

static void GameOnUnload(SD_Application* appPtr, GameState* state) {
  (void)appPtr;
  printf("GAME VERSION %d UNLOADING\n", state->version);
  state->sharedScene = nullptr;
  state->anotherScene = nullptr;
}

// The single exported function

extern "C" GameAPI GetGameAPI(void) {
  GameAPI api = {};
  api.OnLoad = GameOnLoad;
  api.OnUpdate = GameOnUpdate;
  api.OnUnload = GameOnUnload;
  return api;
}

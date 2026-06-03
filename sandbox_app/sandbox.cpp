#include <cstdio>

#include <SD/Application.hpp>
#include <SD/core/SceneView.hpp>
#include <SD/core/ecs/components.hpp>
#include <SD/core/layers/EngineDebugLayer.hpp>
#include <SD/core/vulkan/VulkanRenderer.hpp>
#include <SD/game.h>

#include "GameRenderLayer.hpp"
#include "logging.hpp"

// Wrap sd types for C interface
static sd::Application* to_app(SD_Application* app) {
  return reinterpret_cast<sd::Application*>(app);
}
static sd::Scene* to_scene(SD_Scene* scene) {
  return reinterpret_cast<sd::Scene*>(scene);
}
[[maybe_unused]] static SD_Application* from_app(sd::Application* app) {
  return reinterpret_cast<SD_Application*>(app);
}
static SD_Scene* from_scene(sd::Scene* scene) {
  return reinterpret_cast<SD_Scene*>(scene);
}

static void register_game_categories() {
  sd::log::register_category("game", ImVec4(0.4f, 1.0f, 0.4f, 1.0f));
  sd::log::register_category("game/combat", ImVec4(1.0f, 0.6f, 0.0f, 1.0f));
  sd::log::register_category("game/ui", ImVec4(1.0f, 0.4f, 0.8f, 1.0f));
  sd::log::register_category("game/audio", ImVec4(0.4f, 0.8f, 1.0f, 1.0f));
}

static void game_on_load(SD_Application* appPtr, GameState* state) {
  register_game_categories();
  sd::Application& app = *to_app(appPtr);

  state->version++;
  printf("GAME VERSION %d LOADED\n", state->version);

  state->shared_scene  = from_scene(app.create_scene("MainScene"));
  state->another_scene = from_scene(app.create_scene("AnotherScene"));

  sd::WindowId main_win{0};
  auto&        ctrl =
      app.push_view_layer<sd::Panel>(main_win, "Sandbox Controls", to_scene(state->another_scene));
  ctrl.set_scene(to_scene(state->shared_scene));

  auto& game_view = app.create_view<sd::View>("Game", app.services());
  auto& renderer  = app.services().renderer;
  auto& pipelines = renderer.get_pipeline_factory();
  game_view.setup_layered_render(3);

  {
    struct Push {
      VLA::Matrix4x4f mvp{};
      float           color[4]{};
    };
    VkPipelineLayout push_layout =
        pipelines.create_push_constant_layout(VK_SHADER_STAGE_VERTEX_BIT, sizeof(Push));

    sd::PipelineFactory::PipelineDesc world_desc{.vert_path   = "assets/engine/shaders/world.vert",
                                                 .frag_path   = "assets/engine/shaders/world.frag",
                                                 .render_pass = game_view.get_layered_render_pass(),
                                                 .subpass     = 1};

    auto world_pipe_handle = pipelines.create_graphics_pipeline(world_desc, push_layout);

    sd::PipelineFactory::PipelineDesc wire_desc = world_desc;
    wire_desc.polygon_mode                      = VK_POLYGON_MODE_LINE;
    auto wire_pipe_handle = pipelines.create_graphics_pipeline(wire_desc, push_layout);

    sd::log::game::info("Pushing GameRenderLayer");
    game_view.push_layer<GameRenderLayer>(1, "GameRender", to_scene(state->shared_scene),
                                          world_pipe_handle, wire_pipe_handle, push_layout,
                                          &pipelines);
  }

  app.push_global_layer<sd::EngineDebugLayer>(app.runtime(), app.services(),
                                              to_scene(state->shared_scene));

  auto scene = to_scene(state->shared_scene);
  auto ent   = scene->em.create();
  scene->em.add_component<sd::DebugName>(ent, "Triangle");
  scene->em.add_component<sd::Transform>(ent, VLA::Matrix4x4f::Identity());
  scene->em.add_component<sd::Renderable>(ent, sd::Renderable{0, 0, 1, 1});


  auto ent2 = scene->em.create();
  scene->em.add_component<sd::DebugName>(ent2, "Better triangle");
  scene->em.add_component<sd::Transform>(ent2, VLA::Matrix4x4f::Identity());
  scene->em.add_component<sd::Renderable>(ent2, sd::Renderable{0, 0, 1, 1});

  sd::log::game::info("Game loaded, version {}", state->version);
}

static void game_on_update(SD_Application* app_ptr, GameState* state, float dt) {
  (void)app_ptr;
  (void)state;
  (void)dt;
}

static void game_on_unload(SD_Application* app_ptr, GameState* state) {
  (void)app_ptr;
  printf("GAME VERSION %d UNLOADING\n", state->version);
  state->shared_scene  = nullptr;
  state->another_scene = nullptr;
}

// The single exported function

extern "C" GameAPI get_game_api(void) {
  GameAPI api     = {};
  api.api_version = GAME_API_VERSION;
  api.struct_size = sizeof(GameAPI);
  api.on_load     = game_on_load;
  api.on_update   = game_on_update;
  api.on_unload   = game_on_unload;
  return api;
}

#include <iostream>
#include <memory>

#include "Application.hpp"
#include "core/ecs/components.hpp"
#include "core/layers/EngineDebugLayer.hpp"
#include "core/vulkan/VulkanRenderer.hpp"
#include "GameContext.hpp"
#include "GameRenderLayer.hpp"
#include "logging.hpp"
#include "RuntimeStateManager.hpp"

static void register_game_categories() {
  sd::log::register_category("game", ImVec4(0.4f, 1.0f, 0.4f, 1.0f));
  sd::log::register_category("game/combat", ImVec4(1.0f, 0.6f, 0.0f, 1.0f));
  sd::log::register_category("game/ui", ImVec4(1.0f, 0.4f, 0.8f, 1.0f));
  sd::log::register_category("game/audio", ImVec4(0.4f, 0.8f, 1.0f, 1.0f));
}

class SandboxGame : public sd::GameContext {
public:
  explicit SandboxGame(sd::RuntimeStateManager* stateManager) : m_state_manager(stateManager) {}

  void on_load(sd::Application& app) override {
    m_shared_scene = app.create_scene("MainScene");
    m_another_scene = app.create_scene("AnotherScene");

    auto& game_view = app.create_view<sd::View>("Game");
    auto& renderer = app.get_renderer();
    auto& pipelines = renderer.get_pipeline_factory();
    game_view.setup_layered_render(3);

    struct Push {
      VLA::Matrix4x4f mvp{};
      float color[4]{};
    };
    VkPipelineLayout push_layout =
        pipelines.create_push_constant_layout(VK_SHADER_STAGE_VERTEX_BIT, sizeof(Push));

    sd::PipelineFactory::PipelineDesc world_desc{.vert_path = "assets/engine/shaders/world.vert",
                                                .frag_path = "assets/engine/shaders/world.frag",
                                                .render_pass = game_view.get_layered_render_pass(),
                                                .subpass = 1};
    VkRenderPass pass = game_view.get_layered_render_pass();
    assert(pass && "Null renderpass");

    auto world_pipe_handle = pipelines.create_graphics_pipeline(world_desc, push_layout);
    assert(world_pipe_handle && "Pipeline creation failed");

    sd::PipelineFactory::PipelineDesc wireDesc = world_desc;
    wireDesc.polygon_mode = VK_POLYGON_MODE_LINE;
    auto wire_pipe_handle = pipelines.create_graphics_pipeline(wireDesc, push_layout);

    game_view.push_layer<GameRenderLayer>(1, "GameRender", m_shared_scene, world_pipe_handle,
                                        wire_pipe_handle, push_layout, &pipelines);
    app.push_global_layer<sd::EngineDebugLayer>(m_shared_scene);

    auto ent = m_shared_scene->em.create();
    m_shared_scene->em.add_component<sd::DebugName>(ent, "Triangle");
    m_shared_scene->em.add_component<sd::Transform>(ent, VLA::Matrix4x4f::Identity());
    m_shared_scene->em.add_component<sd::Renderable>(ent, sd::Renderable{0, 0, 1, 1});

    auto ent2 = m_shared_scene->em.create();
    auto mat2 = VLA::Matrix4x4f::Identity();

    mat2.Column(3) = {3, 0, 0, 1};
    m_shared_scene->em.add_component<sd::DebugName>(ent2, "Better triangle");
    m_shared_scene->em.add_component<sd::Transform>(ent2, VLA::Matrix4x4f::Identity());
    m_shared_scene->em.add_component<sd::Renderable>(ent2, sd::Renderable{0, 0, 1, 1});

    if (m_state_manager && m_state_manager->has_state()) {
      m_state_manager->restore(&app);
    }
  }

  void on_update(float dt) override { (void)dt; }

  void on_unload() override {
    m_shared_scene = nullptr;
    m_another_scene = nullptr;
  }

  std::vector<sd::Scene*> get_scenes() override {
    std::vector<sd::Scene*> scenes;
    if (m_shared_scene)
      scenes.push_back(m_shared_scene);
    if (m_another_scene && m_another_scene != m_shared_scene)
      scenes.push_back(m_another_scene);
    return scenes;
  }

private:
  sd::RuntimeStateManager* m_state_manager;
  sd::Scene* m_shared_scene = nullptr;
  sd::Scene* m_another_scene = nullptr;
};

int main() {
  sd::log::init();
  register_game_categories();

  sd::log::game::info("Starting game");

  std::unique_ptr<sd::RuntimeStateManager> state_manager;
  state_manager = std::make_unique<sd::RuntimeStateManager>();

  sd::ApplicationSpecification spec;
  spec.name = "Sandbox";
  spec.width = 1280;
  spec.height = 720;
  spec.enableHotReload = false;

  std::unique_ptr<sd::Application> app;
  app = std::make_unique<sd::Application>(spec, state_manager.get());

  state_manager->set_application(app.get());

  std::unique_ptr<sd::GameContext> game;
  game = std::make_unique<SandboxGame>(state_manager.get());

  app->set_game_context(game.get());

  game->on_load(*app);

  std::atomic stop_flag{false};
  app->run(&stop_flag);

  return 0;
}

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
  sd::log::game::info("GAME VERSION {} LOADED", state->version);

  state->shared_scene  = from_scene(app.create_scene("MainScene"));
  state->another_scene = from_scene(app.create_scene("AnotherScene"));

  sd::WindowId main_win{0};
  auto&        ctrl =
      app.push_view_layer<sd::Panel>(main_win, "Sandbox Controls", to_scene(state->another_scene));
  ctrl.set_scene(to_scene(state->shared_scene));

  auto& game_view = app.create_view<sd::View>("Game", app.services());
  // auto& renderer       = app.services().renderer;
  auto& vulkan_context = app.services().vulkan;
  game_view.setup_layered_render(3);

  {
    //~ World Pipeline
    struct Push {
      VLA::Matrix4x4f mvp{};
      float           color[4]{};
    };
    //  check that push is smaller than max size
    auto                         physical_device = vulkan_context.get_physical_device();
    vk::PhysicalDeviceProperties props           = physical_device.getProperties();
    u32                          max_push_size   = props.limits.maxPushConstantsSize;
    if (max_push_size < sizeof(Push)) {
      sd::log::game::error("Push size {} exceeds maxPushConstantsSize{}", sizeof(Push),
                           max_push_size);
    }

    //~ create push layout
    vk::UniquePipelineLayout pipeline_layout;
    {
      vk::PushConstantRange push_constant_range{
          .stageFlags = vk::ShaderStageFlagBits::eVertex,
          .size       = static_cast<u32>(sizeof(Push)),
      };
      vk::PipelineLayoutCreateInfo pipeline_layout_info{
          .setLayoutCount         = 0,
          .pSetLayouts            = nullptr,
          .pushConstantRangeCount = 1,
          .pPushConstantRanges    = &push_constant_range};


      auto res =
          vulkan_context.get_vulkan_device().get().createPipelineLayoutUnique(pipeline_layout_info);
      if (res.result != vk::Result::eSuccess) {
        sd::log::engine::critical("Failed to create pipeline layout");
        return;
      }
      pipeline_layout = std::move(res.value);
    }


    const char* vert_path{"assets/engine/shaders/world.vert"};
    const char* frag_path{"assets/engine/shaders/world.frag"};


    // TODO: prolly shouldnt be null
    vk::RenderPass  render_pass{VK_NULL_HANDLE};
    u32             subpass{0};
    vk::PolygonMode polygon_mode{VK_POLYGON_MODE_FILL};

    vk::UniquePipeline pipeline;


    //~ Compile vert and frag modules
    sd::ShaderCompiler shader_compiler;

    vk::UniqueShaderModule vert_module;
    {
      std::vector<u32> spv;
      if (!shader_compiler.compile_shader(vert_path, spv, "vs_6_0")) {
        sd::log::game::error("Failed to compile vertex shader: {}", vert_path);
        return;
      }

      vk::ShaderModuleCreateInfo create_info{.codeSize = spv.size() * sizeof(u32),
                                             .pCode    = spv.data()};

      auto vulkan_device = vulkan_context.get_vulkan_device().get();
      auto res           = vulkan_device.createShaderModuleUnique(create_info);

      // TODO: this is very recoverable
      if (res.result != vk::Result::eSuccess) {
        sd::log::engine::critical("Shader comp failed");
        return;
      }
      vert_module = std::move(res.value);
    }

    vk::UniqueShaderModule frag_module;
    {
      std::vector<u32> spv;
      if (!shader_compiler.compile_shader(frag_path, spv, "ps_6_0")) {
        sd::log::game::error("Failed to compile frag shader at: {}", frag_path);
        return;
      }

      vk::ShaderModuleCreateInfo create_info{
          .codeSize = spv.size() * sizeof(u32),
          .pCode    = spv.data(),
      };

      auto vulkan_device = vulkan_context.get_vulkan_device().get();
      auto res           = vulkan_device.createShaderModuleUnique(create_info);
      if (res.result != vk::Result::eSuccess) {
        sd::log::engine::critical("Shader comp failed");
        return;
      }
      frag_module = std::move(res.value);
    }

    //~ Define shader stages

    vk::PipelineShaderStageCreateInfo vert_stage{
        .stage  = vk::ShaderStageFlagBits::eVertex,
        .module = *vert_module,
        .pName  = "main",
    };


    vk::PipelineShaderStageCreateInfo frag_stage{
        .stage  = vk::ShaderStageFlagBits::eFragment,
        .module = *frag_module,
        .pName  = "main",
    };

    std::array shader_stages{vert_stage, frag_stage};

    vk::PipelineVertexInputStateCreateInfo vertex_input_info{};

    vk::PipelineInputAssemblyStateCreateInfo input_assembly{
        .topology = vk::PrimitiveTopology::eTriangleList,

    };

    vk::PipelineViewportStateCreateInfo viewport_state{
        .viewportCount = 1,
        .scissorCount  = 1,
    };

    vk::PipelineRasterizationStateCreateInfo rasterizer{
        .polygonMode = polygon_mode,
        .cullMode    = vk::CullModeFlagBits::eBack,
        .frontFace   = vk::FrontFace::eCounterClockwise,
        .lineWidth   = 1.0f,
    };
    vk::PipelineMultisampleStateCreateInfo multisampling{
        .rasterizationSamples = vk::SampleCountFlagBits::e1,
    };

    using enum vk::ColorComponentFlagBits;
    vk::PipelineColorBlendAttachmentState color_blend_attachment{
        .blendEnable    = false,
        .colorWriteMask = eR | eG | eB | eA,
    };

    vk::PipelineColorBlendStateCreateInfo color_blending{.attachmentCount = 1,
                                                         .pAttachments = &color_blend_attachment};

    vk::PipelineDepthStencilStateCreateInfo depth_stencil{
        .depthTestEnable       = true,
        .depthWriteEnable      = true,
        .depthCompareOp        = vk::CompareOp::eLess,
        .depthBoundsTestEnable = false,
        .stencilTestEnable     = false,
    };

    std::array dynamic_states = {vk::DynamicState::eViewport, vk::DynamicState::eScissor};
    vk::PipelineDynamicStateCreateInfo dynamic_state = {
        .dynamicStateCount = dynamic_states.size(),
        .pDynamicStates    = dynamic_states.data(),
    };

    vk::GraphicsPipelineCreateInfo pipeline_info{
        .stageCount          = 2,
        .pStages             = shader_stages.data(),
        .pVertexInputState   = &vertex_input_info,
        .pInputAssemblyState = &input_assembly,
        .pTessellationState  = nullptr,
        .pViewportState      = &viewport_state,
        .pRasterizationState = &rasterizer,
        .pMultisampleState   = &multisampling,
        .pDepthStencilState  = &depth_stencil,
        .pColorBlendState    = &color_blending,
        .pDynamicState       = &dynamic_state,
        .layout              = *pipeline_layout,
        .renderPass          = render_pass,
        .subpass             = subpass,
        .basePipelineHandle  = {},
        .basePipelineIndex   = {},
    };


    //~ pipeline cache
    vk::UniquePipelineCache     pipeline_cache{};
    vk::PipelineCacheCreateInfo pipeline_cache_info{};
    {
      auto res = vulkan_context.get_vulkan_device()->createPipelineCacheUnique(pipeline_cache_info);
      if (res.result == vk::Result::eSuccess) {
        pipeline_cache = std::move(res.value);
      } else {
        sd::log::engine::warn("Failed to create pipeline cache");
      }
    }
    //~ pipeline creation:
    auto res = vulkan_context.get_vulkan_device()->createGraphicsPipelinesUnique(*pipeline_cache,
                                                                                 pipeline_info);
    if (res.result != vk::Result::eSuccess) {
      sd::log::engine::critical("Rip, we failed, error is: {} ", static_cast<u32>(res.result));
    } else {
      pipeline = std::move(res.value.front());
    }

    // TODO: then same for wire


    // sd::PipelineFactory::PipelineDesc wire_desc = world_desc;
    // wire_desc.polygon_mode = VK_POLYGON_MODE_LINE;
    // auto wire_pipe_handle = pipelines.create_graphics_pipeline(wire_desc, push_layout);

    sd::log::game::info("Pushing GameRenderLayer");
    // game_view.push_layer<GameRenderLayer>(1, "GameRender", to_scene(state->shared_scene),
    // world_pipe_handle, wire_pipe_handle, push_layout,
    // &pipelines);
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
  sd::log::game::info("GAME VERSION {} UNLOADING", state->version);
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

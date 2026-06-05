#include <cstdio>

#include <SD/Application.hpp>
#include <SD/core/SceneView.hpp>
#include <SD/core/ecs/components.hpp>
#include <SD/core/layers/EngineDebugLayer.hpp>
#include <SD/core/vulkan/VulkanRenderer.hpp>
#include <SD/game_api.hpp>

#include "logging.hpp"

static void register_game_categories() {
  sd::log::register_category("game", ImVec4(0.4f, 1.0f, 0.4f, 1.0f));
  sd::log::register_category("game/combat", ImVec4(1.0f, 0.6f, 0.0f, 1.0f));
  sd::log::register_category("game/ui", ImVec4(1.0f, 0.4f, 0.8f, 1.0f));
  sd::log::register_category("game/audio", ImVec4(0.4f, 0.8f, 1.0f, 1.0f));
}

namespace game {

void on_load(sd::Application& app, State& state) {
  register_game_categories();

  state.version++;
  sd::log::game::info("GAME VERSION {} LOADED", state.version);

  state.shared_scene  = app.create_scene("MainScene");
  state.another_scene = app.create_scene("AnotherScene");

  sd::WindowId main_win{0};
  auto& ctrl = app.push_view_layer<sd::Panel>(main_win, "Sandbox Controls", state.another_scene);
  ctrl.set_scene(state.shared_scene);

  auto& game_view      = app.create_view<sd::View>("Game", app.services());
  auto& vulkan_context = app.services().vulkan;
  game_view.setup_layered_render(3);

  {
    //~ World Pipeline
    struct Push {
      VLA::Matrix4x4f mvp{};
      float           color[4]{};
    };
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

    auto                            surface_format = vulkan_context.get_surface_format().format;
    auto                            depth_format   = game_view.find_depth_format();
    vk::PipelineRenderingCreateInfo rendering_info{.colorAttachmentCount    = 1,
                                                   .pColorAttachmentFormats = &surface_format,
                                                   .depthAttachmentFormat   = depth_format,
                                                   .stencilAttachmentFormat = {}};

    vk::GraphicsPipelineCreateInfo pipeline_info{
        .pNext               = &rendering_info,
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

    sd::log::game::info("Pipeline created");
  }

  app.push_global_layer<sd::EngineDebugLayer>(app.runtime(), app.services(), state.shared_scene);

  auto ent = state.shared_scene->em.create();
  state.shared_scene->em.add_component<sd::DebugName>(ent, "Triangle");
  state.shared_scene->em.add_component<sd::Transform>(ent, VLA::Matrix4x4f::Identity());
  state.shared_scene->em.add_component<sd::Renderable>(ent, sd::Renderable{0, 0, 1, 1});

  auto ent2 = state.shared_scene->em.create();
  state.shared_scene->em.add_component<sd::DebugName>(ent2, "Better triangle");
  state.shared_scene->em.add_component<sd::Transform>(ent2, VLA::Matrix4x4f::Identity());
  state.shared_scene->em.add_component<sd::Renderable>(ent2, sd::Renderable{0, 0, 1, 1});

  sd::log::game::info("Game loaded, version {}", state.version);
}

void on_update(sd::Application& app, State& state, float dt) {
  (void)app;
  (void)state;
  (void)dt;
}

void on_unload(sd::Application& app, State& state) {
  (void)app;
  sd::log::game::info("GAME VERSION {} UNLOADING", state.version);
  state.shared_scene  = nullptr;
  state.another_scene = nullptr;
}

} // namespace game

// C-linkage wrappers for the hot-reloader (dlopen path)

static void c_on_load(SD_Application* app, GameState* state) {
  game::State s;
  s.shared_scene  = reinterpret_cast<sd::Scene*>(state->shared_scene);
  s.another_scene = reinterpret_cast<sd::Scene*>(state->another_scene);
  s.version       = state->version;
  game::on_load(*reinterpret_cast<sd::Application*>(app), s);
  state->shared_scene  = reinterpret_cast<SD_Scene*>(s.shared_scene);
  state->another_scene = reinterpret_cast<SD_Scene*>(s.another_scene);
  state->version       = s.version;
}

static void c_on_update(SD_Application* app, GameState* state, float dt) {
  game::State s;
  s.shared_scene  = reinterpret_cast<sd::Scene*>(state->shared_scene);
  s.another_scene = reinterpret_cast<sd::Scene*>(state->another_scene);
  s.version       = state->version;
  game::on_update(*reinterpret_cast<sd::Application*>(app), s, dt);
}

static void c_on_unload(SD_Application* app, GameState* state) {
  game::State s;
  s.shared_scene  = reinterpret_cast<sd::Scene*>(state->shared_scene);
  s.another_scene = reinterpret_cast<sd::Scene*>(state->another_scene);
  s.version       = state->version;
  game::on_unload(*reinterpret_cast<sd::Application*>(app), s);
}

extern "C" GameAPI get_game_api(void) {
  GameAPI api{};
  api.api_version = GAME_API_VERSION;
  api.struct_size = sizeof(GameAPI);
  api.on_load     = c_on_load;
  api.on_update   = c_on_update;
  api.on_unload   = c_on_unload;
  return api;
}

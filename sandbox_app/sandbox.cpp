#include <filesystem>
#include <fstream>

#include <SD/Application.hpp>
#include <SD/core/ShaderCompiler.hpp>
#include <SD/core/ecs/components.hpp>
#include <SD/core/layers/EngineDebugLayer.hpp>
#include <SD/core/types.hpp>
#include <SD/game_api.hpp>
#include <SD/profiler.hpp>

#include "GameRenderLayer.hpp"
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

  //~ pipeline creation stuff
  sd::WindowId main_win{0};
  auto& ctrl = app.push_window_layer<sd::Panel>(main_win, "Sandbox Controls", state.another_scene);
  ctrl.set_scene(state.shared_scene);

  auto& game_view      = app.create_view<sd::View>("Game", app.services());
  auto& vulkan_context = app.services().vulkan;
  game_view.create_viewport();

  // NOTE: Pipeline is baked from shadermodules, swapchains and everything, so the rest of the stuff
  // below we can keep between pipelines really. and just the pipelieninfo changing slightly

  vk::PolygonMode          polygon_mode{VK_POLYGON_MODE_FILL}; // rasterizer
  vk::UniquePipeline       pipeline;
  vk::UniquePipelineLayout pipeline_layout;

  // NOTE: Should pipeline creation be templated so we can have different push structs? Like can
  //  pipelines have different push constants etc, or should we find something standardized. Most
  //  likely some pipelines will be created dynamically but maybe its smart to have like 3 or 4
  //  different kinds based on whats needed. Or should we just have different functions for those
  //  different pipelines, since we will have compute shaders etc as well. Maybe just make this
  //  beneath into some pipeline creation function, and rather create new ones where needed. and
  //  abstract shared things into other functions later.
  // TODO: compress pipeline creation

  // NOTE: What should be changable between pipelines:
  //  Shader Stages
  //  Vertex input
  //  Input Assembly
  //  Rasterizer
  // MultiSampling
  // Depth/stencil
  // Blend attachments
  // dynamic state
  // color depth formats
  // [pipeline layouts


  // What changes between different pipelines
  // Shaders and shader stages.
  // Push Constants


  const char* vert_path{"assets/engine/shaders/world.vert"};
  const char* frag_path{"assets/engine/shaders/world.frag"};


  vk::UniqueShaderModule vert_module;
  {
    PROFILE("vert_module");
    auto spv = sd::compile_shader(vert_path);
    if (!spv) {
      sd::log::game::error("Failed to compile vertex shader: {}", vert_path);
      return;
    }

    vk::ShaderModuleCreateInfo create_info{.codeSize = spv->size() * sizeof(U32),
                                           .pCode    = spv->data()};

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
    PROFILE("frag module");
    auto spv = sd::compile_shader(frag_path);
    if (!spv) {
      sd::log::game::error("Failed to compile frag shader at: {}", frag_path);
      return;
    }

    vk::ShaderModuleCreateInfo create_info{
        .codeSize = spv->size() * sizeof(U32),
        .pCode    = spv->data(),
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

  struct Push {
    VLA::Matrix4x4f mvp{};
    F32             color[4]{};
  };
  static_assert(sizeof(Push) <= 128, "Push must fit in vulkan spec minimum");

  vk::PushConstantRange push_constant_range{
      .stageFlags = vk::ShaderStageFlagBits::eVertex,
      .size       = static_cast<U32>(sizeof(Push)),
  };

  vk::DescriptorSetLayoutBinding binding1 = {

  };

  std::array descriptor_set_layout_bindings{binding1};

  vk::DescriptorSetLayoutCreateInfo descriptor_set_layout_info{
      .bindingCount = descriptor_set_layout_bindings.size(),
      .pBindings    = descriptor_set_layout_bindings.data()};

  auto res = vulkan_context.get_vulkan_device()->createDescriptorSetLayoutUnique(
      descriptor_set_layout_info);
  if (res.result != vk::Result::eSuccess) {
    sd::log::engine::critical("Failed to create descriptor set layout");
  }

  std::array descriptor_set_layouts{(std::move(res.value))};

  struct VertexPNUV {
    VLA::Vector3f position;
    VLA::Vector3f normal;
    VLA::Vector2f uv;

    static constexpr std::array<vk::VertexInputBindingDescription, 1> binding_descriptions() {
      return {{{.binding   = 0,
                .stride    = sizeof(VertexPNUV),
                .inputRate = vk::VertexInputRate::eVertex}}};
    }

    static constexpr std::array<vk::VertexInputAttributeDescription, 3> attribute_descriptions() {
      return {
          {
           {
                  .location = 0,
                  .binding  = 0,
                  .format   = vk::Format::eR32G32B32Sfloat,
                  .offset   = static_cast<U32>(offsetof(VertexPNUV, position)),
              }, {
                  .location = 1,
                  .binding  = 0,
                  .format   = vk::Format::eR32G32B32Sfloat,
                  .offset   = static_cast<U32>(offsetof(VertexPNUV, normal)),
              }, {
                  .location = 2,
                  .binding  = 0,
                  .format   = vk::Format::eR32G32Sfloat,
                  .offset   = static_cast<U32>(offsetof(VertexPNUV, uv)),
              }, }
      };
    };
  };

  auto binding_descriptions   = VertexPNUV::binding_descriptions();
  auto attribute_descriptions = VertexPNUV::attribute_descriptions();
  {
    PROFILE("Pipeline creation");

    {
      std::vector<vk::DescriptorSetLayout> raw_layouts;
      raw_layouts.reserve(descriptor_set_layouts.size());
      for (auto& layout : descriptor_set_layouts) {
        raw_layouts.push_back(*layout);
      }

      // NOTE: This might be sharted accross pipelines, should be cached or reused when descritpor
      // set layout list and push constant ranges are identical
      vk::PipelineLayoutCreateInfo pipeline_layout_info{
          .setLayoutCount         = descriptor_set_layouts.size(),
          .pSetLayouts            = raw_layouts.data(),
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


    vk::PipelineVertexInputStateCreateInfo vertex_input_info{
        .vertexBindingDescriptionCount   = binding_descriptions.size(),
        .pVertexBindingDescriptions      = binding_descriptions.data(),
        .vertexAttributeDescriptionCount = attribute_descriptions.size(),
        .pVertexAttributeDescriptions    = attribute_descriptions.data(),

    };

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


    auto                            color_format = game_view.get_color_format();
    vk::PipelineRenderingCreateInfo rendering_info{
        .colorAttachmentCount    = 1,
        .pColorAttachmentFormats = &color_format,
        .depthAttachmentFormat   = game_view.get_depth_format(),
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
        .renderPass          = nullptr, // dynamic rendering requires renderPass to be nullptr
        .subpass             = {},      // dynamic rendering requires subpass to be 0
        .basePipelineHandle  = {},
        .basePipelineIndex   = {},
    };

    // TODO: check this properly


    vk::UniquePipelineCache pipeline_cache{};
    {
      PROFILE("pipeline_cache");

      // Load cached pipeline data from previous run if available
      std::vector<char> cache_data;
      std::ifstream     cache_file("cache/pipeline.spv", std::ios::binary | std::ios::ate);
      if (cache_file) {
        auto size = static_cast<U64>(cache_file.tellg());
        cache_file.seekg(0);
        cache_data.resize(static_cast<USize>(size));
        cache_file.read(cache_data.data(), static_cast<std::streamsize>(size));
        sd::log::game::info("Loaded pipeline cache ({} bytes)", size);
        // printf("Loaded pipeline cache (%lu bytes)\n", size);
      }

      vk::PipelineCacheCreateInfo pipeline_cache_info{
          .initialDataSize = cache_data.size(),
          .pInitialData    = cache_data.data(),
      };

      auto res = vulkan_context.get_vulkan_device()->createPipelineCacheUnique(pipeline_cache_info);
      if (res.result == vk::Result::eSuccess) {
        pipeline_cache = std::move(res.value);
      } else {
        sd::log::engine::warn("Failed to create pipeline cache");
      }
    }


    PROFILE_START("createGraphicsPipelinesUnique creation");
    auto res = vulkan_context.get_vulkan_device()->createGraphicsPipelinesUnique(*pipeline_cache,
                                                                                 pipeline_info);
    PROFILE_END();
    if (res.result != vk::Result::eSuccess) {
      sd::log::engine::critical("Rip, we failed, error is: {} ", static_cast<U32>(res.result));
    } else {
      pipeline = std::move(res.value.front());
    }

    // Save pipeline cache for faster reloads
    if (pipeline_cache) {
      auto&  dev      = vulkan_context.get_vulkan_device().get();
      size_t data_sz  = 0;
      auto   cache_hr = dev.getPipelineCacheData(*pipeline_cache, &data_sz, nullptr);
      if (cache_hr == vk::Result::eSuccess && data_sz > 0) {
        std::vector<uint8_t> data(data_sz);
        cache_hr = dev.getPipelineCacheData(*pipeline_cache, &data_sz, data.data());
        if (cache_hr == vk::Result::eSuccess) {
          std::filesystem::create_directories("cache");
          std::ofstream cache_out("cache/pipeline.spv", std::ios::binary);
          cache_out.write(reinterpret_cast<const char*>(data.data()),
                          static_cast<std::streamsize>(data_sz));
          sd::log::game::info("Saved pipeline cache ({} bytes)", data_sz);
          // printf("Saved pipeline cache (%lu bytes)\n", data_sz);
        }
      }
    }


    sd::log::game::info("Pipeline created");
  }

  app.push_layer<sd::EngineDebugLayer>(app.runtime(), app.services(), state.shared_scene);

  //~ ECS population

  state.view_bit = 1u << game_view.get_view_id().value;

  auto ent = state.shared_scene->em.create();
  state.shared_scene->em.add_component<sd::components::DebugName>(ent, "Triangle");
  state.shared_scene->em.add_component<sd::components::Transform>(
      ent,
      VLA::Matrix4x4f::Scale({0.5f, 0.5f, 0.5f}) *
          VLA::Matrix4x4f::Translation({-0.5f, 0.0f, 0.0f}));
  state.shared_scene->em.add_component<sd::components::Renderable>(ent,
                                                                   sd::components::Renderable{
                                                                       0,
                                                                       1,
                                                                       0,
                                                                       ~0u,
                                                                       {0.0f, 1.0f, 0.0f, 1.0f}
  });

  auto ent2 = state.shared_scene->em.create();
  state.shared_scene->em.add_component<sd::components::DebugName>(ent2, "Better triangle");
  state.shared_scene->em.add_component<sd::components::Transform>(
      ent2,
      VLA::Matrix4x4f::Scale({0.3f, 0.3f, 0.3f}));
  state.shared_scene->em.add_component<sd::components::Renderable>(
      ent2,
      sd::components::Renderable{0, 0, 0, ~0u});

  game_view.push_layer<GameRenderLayer>("Game Render Layer",
                                        state.shared_scene,
                                        std::move(pipeline),
                                        std::move(pipeline_layout));

  sd::log::game::info("Game loaded, version {}", state.version);
}

void on_reload(sd::Application& app, State& state) {
  (void)app;
  register_game_categories();
  state.version++;
  sd::log::game::info("Game {} reloaded", state.version);
  // FIXME: on_reload needs to re-push layers
}

void on_update(sd::Application& app, State& state, float dt) {
  (void)app;
  (void)state;
  (void)dt;
}

void on_unload(sd::Application& app, State& state) {
  (void)app;
  sd::log::game::info("GAME VERSION {} UNLOADING", state.version);
}

} // namespace game

// C-linkage wrappers for the hot-reloader (dlopen path)

static void c_on_load(SD_Application* app, void* state) {
  game::on_load(*reinterpret_cast<sd::Application*>(app), *static_cast<game::State*>(state));
}

static void c_on_update(SD_Application* app, void* state, float dt) {
  game::on_update(*reinterpret_cast<sd::Application*>(app), *static_cast<game::State*>(state), dt);
}

static void c_on_unload(SD_Application* app, void* state) {
  game::on_unload(*reinterpret_cast<sd::Application*>(app), *static_cast<game::State*>(state));
}

static void c_on_reload(SD_Application* app, void* state) {
  game::on_reload(*reinterpret_cast<sd::Application*>(app), *static_cast<game::State*>(state));
}

extern "C" GameAPI get_game_api(void) {
  GameAPI api{};
  api.api_version = GAME_API_VERSION;
  api.struct_size = sizeof(GameAPI);
  api.on_load     = c_on_load;
  api.on_update   = c_on_update;
  api.on_unload   = c_on_unload;
  api.on_reload   = c_on_reload;
  return api;
}

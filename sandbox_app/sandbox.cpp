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
#include "SD/Vertex.hpp"
#include "logging.hpp"

static void register_game_categories() {
  sd::log::register_category("game", ImVec4(0.4f, 1.0f, 0.4f, 1.0f));
  sd::log::register_category("game/combat", ImVec4(1.0f, 0.6f, 0.0f, 1.0f));
  sd::log::register_category("game/ui", ImVec4(1.0f, 0.4f, 0.8f, 1.0f));
  sd::log::register_category("game/audio", ImVec4(0.4f, 0.8f, 1.0f, 1.0f));
}
// in anonymous namespace until i decide where to move it
namespace {
vk::UniqueShaderModule create_shader_module(vk::Device device, std::string_view shader_path) {
  PROFILE((shader_path.data()));
  auto spv{sd::compile_shader(shader_path)};
  if (!spv) {
    sd::log::game::error("Failed to compile vertex shader: {}", shader_path);
    return {};
  }

  vk::ShaderModuleCreateInfo create_info{.codeSize = spv->size() * sizeof(U32),
                                         .pCode    = spv->data()};

  auto res{device.createShaderModuleUnique(create_info)};

  if (res.result != vk::Result::eSuccess) {
    NOT_IMPLEMENTED;
    return {};
  }
  return std::move(res.value);
}
} // namespace

namespace game {

void on_load(sd::Application& app, State& state) {
  register_game_categories();
  state.version++;
  sd::log::game::info("GAME VERSION {} LOADED", state.version);

  state.shared_scene  = app.create_scene("MainScene");
  state.another_scene = app.create_scene("AnotherScene");

  //~ pipeline creation stuff
  sd::WindowId main_win{0};
  sd::Panel&   ctrl{
      app.push_window_layer<sd::Panel>(main_win, "Sandbox Controls", state.another_scene)};

  ctrl.set_scene(state.shared_scene);

  sd::View&          game_view{app.create_view<sd::View>("Game", app.services())};
  sd::VulkanContext& vulkan_context{app.services().vulkan};
  game_view.create_viewport();

  // NOTE: Pipeline is baked from shadermodules, swapchains and everything, so the rest of the stuff
  // below we can keep between pipelines really. and just the pipelieninfo changing slightly

  vk::PolygonMode          polygon_mode{VK_POLYGON_MODE_FILL}; // rasterizer
  vk::UniquePipeline       pipeline{};
  vk::UniquePipelineLayout pipeline_layout{};
  {
    const vk::Device vulkan_device{*vulkan_context.get_vulkan_device()};
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


    vk::UniqueShaderModule vert_module{create_shader_module(vulkan_device, vert_path)};
    if (!vert_module) {
      return;
    }
    vk::UniqueShaderModule frag_module{create_shader_module(vulkan_device, frag_path)};
    if (!frag_module) {
      return;
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


    std::array descriptor_set_layouts{[&] {
      auto res{vulkan_device.createDescriptorSetLayoutUnique(descriptor_set_layout_info)};
      if (res.result != vk::Result::eSuccess) {
        NOT_IMPLEMENTED;
      }
      return std::move(res.value);
    }()};


    static_assert(std::is_standard_layout_v<SD::VertexPNUV>);
    static_assert(sizeof(VLA::Vector3f) == 12);
    static_assert(sizeof(VLA::Vector2f) == 8);
    static_assert(offsetof(SD::VertexPNUV, position) == 0);
    static_assert(offsetof(SD::VertexPNUV, normal) == 12);
    static_assert(offsetof(SD::VertexPNUV, uv) == 24);
    static_assert(sizeof(SD::VertexPNUV) == 32);


    auto binding_descriptions{SD::VertexPNUV::binding_descriptions()};
    auto attribute_descriptions{SD::VertexPNUV::attribute_descriptions()};
    {
      PROFILE("Pipeline creation");

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

      auto res{vulkan_device.createPipelineLayoutUnique(pipeline_layout_info)};
      if (res.result != vk::Result::eSuccess) {
        NOT_IMPLEMENTED;
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

    std::array dynamic_states{vk::DynamicState::eViewport, vk::DynamicState::eScissor};
    vk::PipelineDynamicStateCreateInfo dynamic_state = {
        .dynamicStateCount = dynamic_states.size(),
        .pDynamicStates    = dynamic_states.data(),
    };


    vk::Format                      color_format{game_view.get_color_format()};
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


    // TODO: this should be reworked
    vk::UniquePipelineCache pipeline_cache{};
    {
      PROFILE("pipeline_cache");

      // Load cached pipeline data from previous run if available
      std::vector<char> cache_data{};
      std::ifstream     cache_file("cache/pipeline.spv", std::ios::binary | std::ios::ate);
      if (cache_file) {
        std::streamsize size(cache_file.tellg());

        cache_file.seekg(0);
        cache_data.resize(size);
        cache_file.read(cache_data.data(), size);
        sd::log::game::info("Loaded pipeline cache ({} bytes)", size);
        // printf("Loaded pipeline cache (%lu bytes)\n", size);
      }

      vk::PipelineCacheCreateInfo pipeline_cache_info{
          .initialDataSize = cache_data.size(),
          .pInitialData    = cache_data.data(),
      };

      auto res{vulkan_device.createPipelineCacheUnique(pipeline_cache_info)};
      if (res.result == vk::Result::eSuccess) {
        pipeline_cache = std::move(res.value);
      } else {
        sd::log::engine::warn("Failed to create pipeline cache");
      }
    }


    PROFILE_START("createGraphicsPipelinesUnique creation");
    auto res{vulkan_device.createGraphicsPipelinesUnique(*pipeline_cache, pipeline_info)};
    PROFILE_END();
    if (res.result != vk::Result::eSuccess) {
      NOT_IMPLEMENTED;
    }
    pipeline = std::move(res.value.front());

    // Save pipeline cache for faster reloads
    // TODO: needs rework aswell possibly
    if (pipeline_cache) {
      USize data_sz{};
      auto  cache_hr{vulkan_device.getPipelineCacheData(*pipeline_cache, &data_sz, nullptr)};
      if (cache_hr == vk::Result::eSuccess && data_sz > 0) {
        std::vector<uint8_t> data(data_sz);
        cache_hr = vulkan_device.getPipelineCacheData(*pipeline_cache, &data_sz, data.data());
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

  sd::Entity ent{state.shared_scene->em.create()};
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

  sd::Entity ent2{state.shared_scene->em.create()};
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
  return {
      .api_version = GAME_API_VERSION,
      .struct_size = sizeof(GameAPI),
      .on_load     = c_on_load,
      .on_update   = c_on_update,
      .on_unload   = c_on_unload,
      .on_reload   = c_on_reload,
  };
}

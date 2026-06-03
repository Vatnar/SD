#include "SD/core/vulkan/PipelineFactory.hpp"

#include <array>

#include "SD/core/vulkan/vulkan_utils.hpp"
#include "SD/utils/utils.hpp"

namespace sd {
PipelineFactory::PipelineFactory(VkDevice device, ShaderLibrary& shaders) :
  m_device(device), m_shaders(shaders) {
  ASSERT(device && "Device must be valid");
  // Reserve index 0 as null/invalid
  m_tracked_pipelines.emplace_back(); // Entry 0 is null

  // Create pipeline cache for better performance
  vk::PipelineCacheCreateInfo cache_info{};
  auto                        cache_result = m_device.createPipelineCacheUnique(cache_info);
  if (cache_result.result == vk::Result::eSuccess) {
    m_pipeline_cache = std::move(cache_result.value);
  } else {
    log::engine::warn("Failed to create pipeline cache, continuing without caching");
  }
}

PipelineFactory::~PipelineFactory() = default;

PipelineFactory::Handle PipelineFactory::create_graphics_pipeline(const PipelineDesc& desc,
                                                                  VkPipelineLayout    layout) {
  ASSERT(desc.render_pass && "Render pass must be valid");
  ASSERT(layout && "Pipeline layout must be valid");

  vk::UniquePipeline pipeline;
  VK_CHECK(create_pipeline_instance(desc, layout, &pipeline));

  uint32_t index;

  // Reuse a free slot if available
  if (!m_free_slots.empty()) {
    index = m_free_slots.back();
    m_free_slots.pop_back();

    // Reuse the slot, increment generation
    auto& entry    = m_tracked_pipelines[index];
    entry.pipeline = std::move(pipeline);
    entry.desc     = desc;
    entry.layout   = layout;
    entry.generation++; // Increment generation for new use
    entry.active = true;
  } else {
    // Allocate new slot
    index = static_cast<uint32_t>(m_tracked_pipelines.size());
    m_tracked_pipelines.push_back({
        std::move(pipeline), desc, layout,
        1,   // Generation starts at 1
        true // active
    });
  }

  return Handle{index, m_tracked_pipelines[index].generation};
}

VkPipeline PipelineFactory::get_pipeline(Handle handle) const {
  if (!handle || handle.index >= m_tracked_pipelines.size()) {
    return VK_NULL_HANDLE;
  }

  const auto& entry = m_tracked_pipelines[handle.index];
  if (!entry.active || entry.generation != handle.generation) {
    return VK_NULL_HANDLE;
  }

  return entry.pipeline.get();
}

bool PipelineFactory::is_valid(Handle handle) const {
  if (!handle || handle.index >= m_tracked_pipelines.size()) {
    return false;
  }

  const auto& entry = m_tracked_pipelines[handle.index];
  return entry.active && entry.generation == handle.generation;
}

void PipelineFactory::destroy_pipeline(Handle handle) {
  if (!is_valid(handle)) {
    return;
  }

  auto& entry = m_tracked_pipelines[handle.index];
  entry.pipeline.reset(); // Destroy the Vulkan pipeline
  entry.active = false;
  // Note: We don't increment generation here - the handle becomes invalid
  // because active=false. If we reuse this slot, generation will be incremented.

  m_free_slots.push_back(handle.index);
}

VkPipelineLayout PipelineFactory::create_push_constant_layout(vk::ShaderStageFlags stages,
                                                              u32                  size) {
  ASSERT(m_device && "Device must be valid");
  ASSERT(size > 0 && "Push constant size must be > 0");

  vk::PushConstantRange push_constant_range{.stageFlags = stages, .offset = 0, .size = size};

  vk::PipelineLayoutCreateInfo pipeline_layout_info{.setLayoutCount         = 0,
                                                    .pSetLayouts            = nullptr,
                                                    .pushConstantRangeCount = 1,
                                                    .pPushConstantRanges    = &push_constant_range};


  vk::UniquePipelineLayout pipeline_layout =
      check_vulkan_res_val(m_device.createPipelineLayoutUnique(pipeline_layout_info),
                           "Failed to create pipeline layout");

  VkPipelineLayout raw = pipeline_layout.get();
  m_created_layouts.push_back(std::move(pipeline_layout));
  return raw;
}

std::vector<std::pair<std::string, std::string>> PipelineFactory::recreate_all_pipelines() {
  ASSERT(m_device && "Device must be valid");

  std::vector<std::pair<std::string, std::string>> failures;
  for (auto& entry : m_tracked_pipelines) {
    if (!entry.active)
      continue;

    vk::UniquePipeline new_pipeline;
    VkResult           result = create_pipeline_instance(entry.desc, entry.layout, &new_pipeline);

    if (result == VK_SUCCESS) {
      entry.pipeline = std::move(new_pipeline);
      // Note: Generation stays the same, handles remain valid
    } else {
      failures.emplace_back(entry.desc.vert_path, entry.desc.frag_path);
      sd::log::engine::error("Failed to recreate pipeline during hot-reload for shader: {} / {}",
                             entry.desc.vert_path, entry.desc.frag_path);
      // Continue with other pipelines instead of aborting
    }
  }
  return failures;
}

VkResult PipelineFactory::create_pipeline_instance([[maybe_unused]] const PipelineDesc& desc,
                                                   [[maybe_unused]] vk::PipelineLayout  layout,
                                                   [[maybe_unused]] vk::UniquePipeline* pipeline) {
  // ASSERT(m_device && "Device must be valid");
  // ASSERT(pipeline && "Pipeline output pointer must be valid");
  // ASSERT(!desc.vert_path.empty() && "Vertex shader path must be valid");
  // ASSERT(!desc.frag_path.empty() && "Fragment shader path must be valid");
  //
  // VkShaderModule vert_module = m_shaders.load(desc.vert_path, "vs_6_0");
  // VkShaderModule frag_module = m_shaders.load(desc.frag_path, "ps_6_0");
  //
  // if (!vert_module) {
  //   sd::log::engine::error("Failed to load vertex shader: {}", desc.vert_path);
  //   return VK_ERROR_INITIALIZATION_FAILED;
  // }
  // if (!frag_module) {
  //   sd::log::engine::error("Failed to load fragment shader: {}", desc.frag_path);
  //   return VK_ERROR_INITIALIZATION_FAILED;
  // }
  //
  // VkPipelineShaderStageCreateInfo shader_stages[] = {
  //     {.sType               = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
  //      .pNext               = {},
  //      .flags               = {},
  //      .stage               = VK_SHADER_STAGE_VERTEX_BIT,
  //      .module              = vert_module,
  //      .pName               = "main",
  //      .pSpecializationInfo = {}},
  //     {.sType               = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
  //      .pNext               = {},
  //      .flags               = {},
  //      .stage               = VK_SHADER_STAGE_FRAGMENT_BIT,
  //      .module              = frag_module,
  //      .pName               = "main",
  //      .pSpecializationInfo = {}}
  // };
  //
  // VkPipelineVertexInputStateCreateInfo vertex_input_info = {
  //     .sType                           =
  //     VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
  //     .pNext                           = {},
  //     .flags                           = {},
  //     .vertexBindingDescriptionCount   = {},
  //     .pVertexBindingDescriptions      = {},
  //     .vertexAttributeDescriptionCount = {},
  //     .pVertexAttributeDescriptions    = {},
  // };
  //
  // VkPipelineInputAssemblyStateCreateInfo input_assembly = {
  //     .sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
  //     .pNext                  = {},
  //     .flags                  = {},
  //     .topology               = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
  //     .primitiveRestartEnable = {},
  // };
  //
  // VkPipelineViewportStateCreateInfo viewport_state = {
  //     .sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
  //     .pNext         = {},
  //     .flags         = {},
  //     .viewportCount = 1,
  //     .pViewports    = {},
  //     .scissorCount  = 1,
  //     .pScissors     = {},
  // };
  //
  // VkPipelineRasterizationStateCreateInfo rasterizer = {
  //     .sType                   = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
  //     .pNext                   = {},
  //     .flags                   = {},
  //     .depthClampEnable        = {},
  //     .rasterizerDiscardEnable = {},
  //     .polygonMode             = desc.polygon_mode,
  //     .cullMode                = VK_CULL_MODE_BACK_BIT,
  //     .frontFace               = VK_FRONT_FACE_COUNTER_CLOCKWISE,
  //     .depthBiasEnable         = {},
  //     .depthBiasConstantFactor = {},
  //     .depthBiasClamp          = {},
  //     .depthBiasSlopeFactor    = {},
  //     .lineWidth               = 1.0f,
  // };
  //
  // VkPipelineMultisampleStateCreateInfo multisampling = {
  //     .sType                 = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
  //     .pNext                 = {},
  //     .flags                 = {},
  //     .rasterizationSamples  = VK_SAMPLE_COUNT_1_BIT,
  //     .sampleShadingEnable   = {},
  //     .minSampleShading      = {},
  //     .pSampleMask           = {},
  //     .alphaToCoverageEnable = {},
  //     .alphaToOneEnable      = {},
  // };
  //
  // VkPipelineColorBlendAttachmentState color_blend_attachment = {
  //     .blendEnable         = VK_FALSE,
  //     .srcColorBlendFactor = {},
  //     .dstColorBlendFactor = {},
  //     .colorBlendOp        = {},
  //     .srcAlphaBlendFactor = {},
  //     .dstAlphaBlendFactor = {},
  //     .alphaBlendOp        = {},
  //     .colorWriteMask      = static_cast<VkColorComponentFlags>(0xf),
  // };
  //
  // VkPipelineColorBlendStateCreateInfo color_blending = {
  //     .sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
  //     .pNext           = {},
  //     .flags           = {},
  //     .logicOpEnable   = {},
  //     .logicOp         = {},
  //     .attachmentCount = 1,
  //     .pAttachments    = &color_blend_attachment,
  //     .blendConstants  = {}};
  //
  // VkPipelineDepthStencilStateCreateInfo depth_stencil = {
  //     .sType                 = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
  //     .pNext                 = {},
  //     .flags                 = {},
  //     .depthTestEnable       = VK_TRUE,
  //     .depthWriteEnable      = VK_TRUE,
  //     .depthCompareOp        = VK_COMPARE_OP_LESS,
  //     .depthBoundsTestEnable = VK_FALSE,
  //     .stencilTestEnable     = VK_FALSE,
  //     .front                 = {},
  //     .back                  = {},
  //     .minDepthBounds        = {},
  //     .maxDepthBounds        = {},
  // };
  //
  // std::array dynamic_states = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
  // VkPipelineDynamicStateCreateInfo dynamic_state = {
  //     .sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
  //     .pNext             = {},
  //     .flags             = {},
  //     .dynamicStateCount = static_cast<u32>(dynamic_states.size()),
  //     .pDynamicStates    = dynamic_states.data(),
  // };
  //
  // VkGraphicsPipelineCreateInfo pipeline_info{
  //     .sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
  //     .pNext               = {},
  //     .flags               = {},
  //     .stageCount          = 2,
  //     .pStages             = shader_stages, // VkPipelineShaderStageCreateInfo[2,
  //     .pVertexInputState   = &vertex_input_info,
  //     .pInputAssemblyState = &input_assembly,
  //     .pTessellationState  = nullptr,
  //     .pViewportState      = &viewport_state,
  //     .pRasterizationState = &rasterizer,
  //     .pMultisampleState   = &multisampling,
  //     .pDepthStencilState  = &depth_stencil,
  //     .pColorBlendState    = &color_blending,
  //     .pDynamicState       = &dynamic_state,
  //     .layout              = layout,
  //     .renderPass          = desc.render_pass,
  //     .subpass             = desc.subpass,
  //     .basePipelineHandle  = {},
  //     .basePipelineIndex   = {},
  // };


  // auto res = m_device.createGraphicsPipelinesUnique(m_pipeline_cache.get(), {pipeline_info});
  // if (res.result != vk::Result::eSuccess) {
  // return static_cast<VkResult>(res.result);
  // }

  // *pipeline = std::move(res.value[0]);
  return VK_SUCCESS;
}
} // namespace sd

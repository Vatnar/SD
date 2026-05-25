#include "Core/Vulkan/PipelineFactory.hpp"

#include <array>

#include "Utils/Utils.hpp"

namespace SD {
PipelineFactory::PipelineFactory(VkDevice device, ShaderLibrary& shaders) :
  mDevice(device), mShaders(shaders) {
  SD_ALWAYS_ASSERT(device, "Device must be valid");
  // Reserve index 0 as null/invalid
  mTrackedPipelines.emplace_back(); // Entry 0 is null
  
  // Create pipeline cache for better performance
  vk::PipelineCacheCreateInfo cacheInfo{};
  auto cacheResult = mDevice.createPipelineCacheUnique(cacheInfo);
  if (cacheResult.result == vk::Result::eSuccess) {
    mPipelineCache = std::move(cacheResult.value);
  } else {
    SD::Log::Engine::Warn("Failed to create pipeline cache, continuing without caching");
  }
}

PipelineFactory::~PipelineFactory() = default;

PipelineFactory::Handle PipelineFactory::CreateGraphicsPipeline(const PipelineDesc& desc,
                                                                VkPipelineLayout layout) {
  SD_ALWAYS_ASSERT(desc.renderPass, "Render pass must be valid");
  SD_ALWAYS_ASSERT(layout, "Pipeline layout must be valid");

  vk::UniquePipeline pipeline;
  VK_CHECK(CreatePipelineInstance(desc, layout, &pipeline));

  uint32_t index;
  
  // Reuse a free slot if available
  if (!mFreeSlots.empty()) {
    index = mFreeSlots.back();
    mFreeSlots.pop_back();
    
    // Reuse the slot, increment generation
    auto& entry = mTrackedPipelines[index];
    entry.pipeline = std::move(pipeline);
    entry.desc = desc;
    entry.layout = layout;
    entry.generation++; // Increment generation for new use
    entry.active = true;
  } else {
    // Allocate new slot
    index = static_cast<uint32_t>(mTrackedPipelines.size());
    mTrackedPipelines.push_back({
      std::move(pipeline),
      desc,
      layout,
      1, // Generation starts at 1
      true // active
    });
  }
  
  return Handle{index, mTrackedPipelines[index].generation};
}

VkPipeline PipelineFactory::GetPipeline(Handle handle) const {
  if (!handle || handle.index >= mTrackedPipelines.size()) {
    return VK_NULL_HANDLE;
  }
  
  const auto& entry = mTrackedPipelines[handle.index];
  if (!entry.active || entry.generation != handle.generation) {
    return VK_NULL_HANDLE;
  }
  
  return entry.pipeline.get();
}

bool PipelineFactory::IsValid(Handle handle) const {
  if (!handle || handle.index >= mTrackedPipelines.size()) {
    return false;
  }
  
  const auto& entry = mTrackedPipelines[handle.index];
  return entry.active && entry.generation == handle.generation;
}

void PipelineFactory::DestroyPipeline(Handle handle) {
  if (!IsValid(handle)) {
    return;
  }
  
  auto& entry = mTrackedPipelines[handle.index];
  entry.pipeline.reset(); // Destroy the Vulkan pipeline
  entry.active = false;
  // Note: We don't increment generation here - the handle becomes invalid
  // because active=false. If we reuse this slot, generation will be incremented.
  
  mFreeSlots.push_back(handle.index);
}

VkPipelineLayout PipelineFactory::CreatePushConstantLayout(VkShaderStageFlags stages, u32 size) {
  SD_ALWAYS_ASSERT(mDevice, "Device must be valid");
  SD_ALWAYS_ASSERT(size > 0, "Push constant size must be > 0");

  vk::PushConstantRange pushConstantRange{static_cast<vk::ShaderStageFlags>(stages), 0, size};

  vk::PipelineLayoutCreateInfo pipelineLayoutInfo{{}, 0, nullptr, 1, &pushConstantRange};

  vk::UniquePipelineLayout pipelineLayout = CheckVulkanResVal(
      mDevice.createPipelineLayoutUnique(pipelineLayoutInfo), "Failed to create pipeline layout");

  VkPipelineLayout raw = pipelineLayout.get();
  mCreatedLayouts.push_back(std::move(pipelineLayout));
  return raw;
}

std::vector<std::pair<std::string, std::string>> PipelineFactory::RecreateAllPipelines() {
  SD_ALWAYS_ASSERT(mDevice, "Device must be valid");

  std::vector<std::pair<std::string, std::string>> failures;
  for (auto& entry : mTrackedPipelines) {
    if (!entry.active) continue;

    vk::UniquePipeline newPipeline;
    VkResult result = CreatePipelineInstance(entry.desc, entry.layout, &newPipeline);

    if (result == VK_SUCCESS) {
      entry.pipeline = std::move(newPipeline);
      // Note: Generation stays the same, handles remain valid
    } else {
      failures.emplace_back(entry.desc.vertPath, entry.desc.fragPath);
      SD::Log::Engine::Error("Failed to recreate pipeline during hot-reload for shader: {} / {}",
                   entry.desc.vertPath, entry.desc.fragPath);
      // Continue with other pipelines instead of aborting
    }
  }
  return failures;
}

VkResult PipelineFactory::CreatePipelineInstance(const PipelineDesc& desc,
                                                 vk::PipelineLayout layout,
                                                 vk::UniquePipeline* pipeline) {
  SD_ALWAYS_ASSERT(mDevice, "Device must be valid");
  SD_ALWAYS_ASSERT(pipeline, "Pipeline output pointer must be valid");
  SD_ALWAYS_ASSERT(!desc.vertPath.empty(), "Vertex shader path must be valid");
  SD_ALWAYS_ASSERT(!desc.fragPath.empty(), "Fragment shader path must be valid");

  VkShaderModule vertModule = mShaders.Load(desc.vertPath, "vs_6_0");
  VkShaderModule fragModule = mShaders.Load(desc.fragPath, "ps_6_0");

  if (!vertModule) {
    SD::Log::Engine::Error("Failed to load vertex shader: {}", desc.vertPath);
    return VK_ERROR_INITIALIZATION_FAILED;
  }
  if (!fragModule) {
    SD::Log::Engine::Error("Failed to load fragment shader: {}", desc.fragPath);
    return VK_ERROR_INITIALIZATION_FAILED;
  }

  VkPipelineShaderStageCreateInfo shaderStages[] = {
      {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
       .pNext = {},
       .flags = {},
       .stage = VK_SHADER_STAGE_VERTEX_BIT,
       .module = vertModule,
       .pName = "main",
       .pSpecializationInfo = {}},
      {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
       .pNext = {},
       .flags = {},
       .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
       .module = fragModule,
       .pName = "main",
       .pSpecializationInfo = {}}
  };

  VkPipelineVertexInputStateCreateInfo vertexInputInfo = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
      .pNext = {},
      .flags = {},
      .vertexBindingDescriptionCount = {},
      .pVertexBindingDescriptions = {},
      .vertexAttributeDescriptionCount = {},
      .pVertexAttributeDescriptions = {},
  };

  VkPipelineInputAssemblyStateCreateInfo inputAssembly = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
      .pNext = {},
      .flags = {},
      .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
      .primitiveRestartEnable = {},
  };

  VkPipelineViewportStateCreateInfo viewportState = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
      .pNext = {},
      .flags = {},
      .viewportCount = 1,
      .pViewports = {},
      .scissorCount = 1,
      .pScissors = {},
  };

  VkPipelineRasterizationStateCreateInfo rasterizer = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
      .pNext = {},
      .flags = {},
      .depthClampEnable = {},
      .rasterizerDiscardEnable = {},
      .polygonMode = desc.polygonMode,
      .cullMode = VK_CULL_MODE_BACK_BIT,
      .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
      .depthBiasEnable = {},
      .depthBiasConstantFactor = {},
      .depthBiasClamp = {},
      .depthBiasSlopeFactor = {},
      .lineWidth = 1.0f,
  };

  VkPipelineMultisampleStateCreateInfo multisampling = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
      .pNext = {},
      .flags = {},
      .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
      .sampleShadingEnable = {},
      .minSampleShading = {},
      .pSampleMask = {},
      .alphaToCoverageEnable = {},
      .alphaToOneEnable = {},
  };

  VkPipelineColorBlendAttachmentState colorBlendAttachment = {
      .blendEnable = VK_FALSE,
      .srcColorBlendFactor = {},
      .dstColorBlendFactor = {},
      .colorBlendOp = {},
      .srcAlphaBlendFactor = {},
      .dstAlphaBlendFactor = {},
      .alphaBlendOp = {},
      .colorWriteMask = static_cast<VkColorComponentFlags>(0xf),
  };

  VkPipelineColorBlendStateCreateInfo colorBlending = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
      .pNext = {},
      .flags = {},
      .logicOpEnable = {},
      .logicOp = {},
      .attachmentCount = 1,
      .pAttachments = &colorBlendAttachment,
      .blendConstants = {}};

  VkPipelineDepthStencilStateCreateInfo depthStencil = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
      .pNext = {},
      .flags = {},
      .depthTestEnable = VK_TRUE,
      .depthWriteEnable = VK_TRUE,
      .depthCompareOp = VK_COMPARE_OP_LESS,
      .depthBoundsTestEnable = VK_FALSE,
      .stencilTestEnable = VK_FALSE,
      .front = {},
      .back = {},
      .minDepthBounds = {},
      .maxDepthBounds = {},
  };

  std::array<VkDynamicState, 2> dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT,
                                                 VK_DYNAMIC_STATE_SCISSOR};
  VkPipelineDynamicStateCreateInfo dynamicState = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
      .pNext = {},
      .flags = {},
      .dynamicStateCount = static_cast<u32>(dynamicStates.size()),
      .pDynamicStates = dynamicStates.data(),
  };

  VkGraphicsPipelineCreateInfo pipelineInfo{
      .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
      .pNext = {},
      .flags = {},
      .stageCount = 2,
      .pStages = shaderStages, // VkPipelineShaderStageCreateInfo[2,
      .pVertexInputState = &vertexInputInfo,
      .pInputAssemblyState = &inputAssembly,
      .pTessellationState = nullptr,
      .pViewportState = &viewportState,
      .pRasterizationState = &rasterizer,
      .pMultisampleState = &multisampling,
      .pDepthStencilState = &depthStencil,
      .pColorBlendState = &colorBlending,
      .pDynamicState = &dynamicState,
      .layout = layout,
      .renderPass = desc.renderPass,
      .subpass = desc.subpass,
      .basePipelineHandle = {},
      .basePipelineIndex = {},
  };


  auto res = mDevice.createGraphicsPipelinesUnique(mPipelineCache.get(), {pipelineInfo});
  if (res.result != vk::Result::eSuccess) {
    return static_cast<VkResult>(res.result);
  }

  *pipeline = std::move(res.value[0]);
  return VK_SUCCESS;
}
} // namespace SD

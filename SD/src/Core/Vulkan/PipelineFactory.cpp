#include "Core/Vulkan/PipelineFactory.hpp"

#include <array>

#include "Utils/Utils.hpp"

namespace SD {

PipelineFactory::PipelineFactory(VkDevice device, ShaderLibrary& shaders) :
  mDevice(device), mShaders(shaders) {
  SD_ASSERT(device, "Device must be valid");
  // Use default constructor (which is empty/null) for UniqueHandle with dynamic dispatcher
}

PipelineFactory::~PipelineFactory() = default;

VkPipeline PipelineFactory::CreateGraphicsPipeline(const PipelineDesc& desc,
                                                   VkPipelineLayout layout) {
  SD_ASSERT(desc.renderPass, "Render pass must be valid");
  SD_ASSERT(layout, "Pipeline layout must be valid");

  vk::UniquePipeline pipeline;
  VK_CHECK(CreatePipelineInstance(desc, layout, &pipeline));

  VkPipeline raw = pipeline.get();
  mTrackedPipelines.push_back({std::move(pipeline), desc, layout});
  return raw;
}

VkPipelineLayout PipelineFactory::CreatePushConstantLayout(VkShaderStageFlags stages, u32 size) {
  SD_ASSERT(mDevice, "Device must be valid");
  SD_ASSERT(size > 0, "Push constant size must be > 0");

  vk::PushConstantRange pushConstantRange{(vk::ShaderStageFlags)stages, 0, size};

  vk::PipelineLayoutCreateInfo pipelineLayoutInfo{{}, 0, nullptr, 1, &pushConstantRange};

  vk::UniquePipelineLayout pipelineLayout = CheckVulkanResVal(
      mDevice.createPipelineLayoutUnique(pipelineLayoutInfo), "Failed to create pipeline layout");

  VkPipelineLayout raw = pipelineLayout.get();
  mCreatedLayouts.push_back(std::move(pipelineLayout));
  return raw;
}

void PipelineFactory::RecreateAllPipelines() {
  SD_ASSERT(mDevice, "Device must be valid");

  for (auto& entry : mTrackedPipelines) {
    if (CreatePipelineInstance(entry.desc, entry.layout, &entry.pipeline) != VkResult::VK_SUCCESS) {
      Abort("Failed to recreate pipeline during hot-reload");
    }
  }
}

VkResult PipelineFactory::CreatePipelineInstance(const PipelineDesc& desc,
                                                  vk::PipelineLayout layout,
                                                  vk::UniquePipeline* pipeline) {
  SD_ASSERT(mDevice, "Device must be valid");
  SD_ASSERT(pipeline, "Pipeline output pointer must be valid");
  SD_ASSERT(!desc.vertPath.empty(), "Vertex shader path must be valid");
  SD_ASSERT(!desc.fragPath.empty(), "Fragment shader path must be valid");

  VkShaderModule vertModule = mShaders.Load(desc.vertPath, "vs_6_0");
  VkShaderModule fragModule = mShaders.Load(desc.fragPath, "ps_6_0");

  SD_ASSERT(vertModule, "Vertex shader module must be valid");
  SD_ASSERT(fragModule, "Fragment shader module must be valid");

  VkPipelineShaderStageCreateInfo shaderStages[] = {
      {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
       .stage = VK_SHADER_STAGE_VERTEX_BIT,
       .module = vertModule,
       .pName = "main"},
      {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
       .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
       .module = fragModule,
       .pName = "main"}
  };

  VkPipelineVertexInputStateCreateInfo vertexInputInfo = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};

  VkPipelineInputAssemblyStateCreateInfo inputAssembly = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
      .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST};

  VkPipelineViewportStateCreateInfo viewportState = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
      .viewportCount = 1,
      .scissorCount = 1};

  VkPipelineRasterizationStateCreateInfo rasterizer = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
      .polygonMode = desc.polygonMode,
      .cullMode = VK_CULL_MODE_BACK_BIT,
      .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
      .lineWidth = 1.0f};

  VkPipelineMultisampleStateCreateInfo multisampling = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
      .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT};

  VkPipelineColorBlendAttachmentState colorBlendAttachment = {
      .blendEnable = VK_FALSE,
      .colorWriteMask = static_cast<VkColorComponentFlags>(0xf)};

  VkPipelineColorBlendStateCreateInfo colorBlending = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
      .attachmentCount = 1,
      .pAttachments = &colorBlendAttachment};

  VkPipelineDepthStencilStateCreateInfo depthStencil = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
      .depthTestEnable = VK_TRUE,
      .depthWriteEnable = VK_TRUE,
      .depthCompareOp = VK_COMPARE_OP_LESS,
      .depthBoundsTestEnable = VK_FALSE,
      .stencilTestEnable = VK_FALSE};

  std::array<VkDynamicState, 2> dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT,
                                                 VK_DYNAMIC_STATE_SCISSOR};
  VkPipelineDynamicStateCreateInfo dynamicState = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
      .dynamicStateCount = (u32)dynamicStates.size(),
      .pDynamicStates = dynamicStates.data()};

  VkGraphicsPipelineCreateInfo pipelineInfo{};
  pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  pipelineInfo.stageCount = 2;
  pipelineInfo.pStages = shaderStages; // VkPipelineShaderStageCreateInfo[2]
  pipelineInfo.pVertexInputState = &vertexInputInfo;
  pipelineInfo.pInputAssemblyState = &inputAssembly;
  pipelineInfo.pTessellationState = nullptr;
  pipelineInfo.pViewportState = &viewportState;
  pipelineInfo.pRasterizationState = &rasterizer;
  pipelineInfo.pMultisampleState = &multisampling;
  pipelineInfo.pDepthStencilState = &depthStencil;
  pipelineInfo.pColorBlendState = &colorBlending;
  pipelineInfo.pDynamicState = &dynamicState;
  pipelineInfo.layout = layout;
  pipelineInfo.renderPass = desc.renderPass;
  pipelineInfo.subpass = desc.subpass;


  auto res = mDevice.createGraphicsPipelinesUnique(mPipelineCache.get(), {pipelineInfo});
  if (res.result != vk::Result::eSuccess) {
    return (VkResult)res.result;
  }

  *pipeline = std::move(res.value[0]);
  return VK_SUCCESS;
}

} // namespace SD

// TODO(docs): Add file-level Doxygen header
//   - @file PipelineFactory.hpp
//   - @brief Graphics pipeline creation and caching
//   - Integration with ShaderLibrary for hot reload
#pragma once

#include <string>
#include <vector>

#include "Core/Base.hpp"
#include "ShaderLibrary.hpp"
#include "VulkanConfig.hpp"

namespace SD {

// TODO(docs): Document PipelineFactory class
//   - Purpose: Creates and caches Vulkan graphics pipelines
//   - PipelineDesc structure and how to use it
//   - Pipeline layout creation with push constants
//   - Hot reload support (RecreateAllPipelines)
//   - Pipeline cache usage
//   - Example: Creating a basic pipeline
/// Creates and owns graphics pipelines. Handles cleanup on destruction.
class PipelineFactory {
public:
  explicit PipelineFactory(VkDevice device, ShaderLibrary& shaders);
  ~PipelineFactory();

  struct PipelineDesc {
    std::string vertPath, fragPath;
    VkRenderPass renderPass = VK_NULL_HANDLE;
    u32 subpass = 0;
    VkPolygonMode polygonMode = VK_POLYGON_MODE_FILL;

    ~PipelineDesc() = default;
    auto operator<=>(const PipelineDesc&) const = default;
  };

  VkPipeline CreateGraphicsPipeline(const PipelineDesc& desc, VkPipelineLayout layout);
  VkPipelineLayout CreatePushConstantLayout(VkShaderStageFlags stages, u32 size);

  /// Recreates all tracked pipelines.
  /// Usually called after clearing ShaderLibrary cache.
  void RecreateAllPipelines();
  VkResult CreatePipelineInstance(const PipelineDesc& desc, vk::PipelineLayout layout,
                                  vk::UniquePipeline* pipeline);

  PipelineFactory(const PipelineFactory&) = delete;
  PipelineFactory& operator=(const PipelineFactory&) = delete;

private:
  VkResult CreatePipelineInstance(const PipelineDesc& desc, VkPipelineLayout layout,
                                  VkPipeline* pipeline);

  vk::Device mDevice;
  ShaderLibrary& mShaders;
  vk::UniquePipelineCache mPipelineCache;

  struct PipelineEntry {
    vk::UniquePipeline pipeline;
    PipelineDesc desc;
    VkPipelineLayout layout;
  };

  std::vector<PipelineEntry> mTrackedPipelines;
  std::vector<vk::UniquePipelineLayout> mCreatedLayouts;
};

} // namespace SD

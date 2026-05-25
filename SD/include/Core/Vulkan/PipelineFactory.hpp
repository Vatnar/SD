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
  /// Stable handle to a pipeline. Remains valid across hot-reloads.
  /// Uses generational indexing to detect use-after-free of destroyed handles.
  struct Handle {
    uint32_t index = 0;      // Slot index in the factory (0 = null/invalid)
    uint32_t generation = 0; // Generation counter for the slot
    
    explicit operator bool() const { return index != 0; }
    bool operator==(const Handle& other) const {
      return index == other.index && generation == other.generation;
    }
    bool operator!=(const Handle& other) const { return !(*this == other); }
  };
  
  static constexpr Handle NullHandle{0, 0};

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

  /// Create a graphics pipeline and return a stable handle.
  /// The handle remains valid even after RecreateAllPipelines() is called.
  Handle CreateGraphicsPipeline(const PipelineDesc& desc, VkPipelineLayout layout);
  
  /// Get the current VkPipeline for a handle.
  /// Returns VK_NULL_HANDLE if handle is invalid or pipeline was destroyed.
  VkPipeline GetPipeline(Handle handle) const;
  
  /// Check if a handle is valid (exists and generation matches).
  bool IsValid(Handle handle) const;
  
  /// Destroy a pipeline and invalidate its handle.
  /// Subsequent GetPipeline() calls will return VK_NULL_HANDLE.
  void DestroyPipeline(Handle handle);

  VkPipelineLayout CreatePushConstantLayout(VkShaderStageFlags stages, u32 size);

  /// Recreates all tracked pipelines.
  /// Usually called after clearing ShaderLibrary cache.
  /// Handles remain valid - they resolve to the new pipeline instances.
  /// @return Vector of failed shader path pairs (vert, frag) for any pipelines that failed to recreate.
  std::vector<std::pair<std::string, std::string>> RecreateAllPipelines();

  PipelineFactory(const PipelineFactory&) = delete;
  PipelineFactory& operator=(const PipelineFactory&) = delete;

private:
  VkResult CreatePipelineInstance(const PipelineDesc& desc, vk::PipelineLayout layout,
                                  vk::UniquePipeline* pipeline);

  struct PipelineEntry {
    vk::UniquePipeline pipeline;
    PipelineDesc desc;
    vk::PipelineLayout layout;
    uint32_t generation = 1; // Generation for this slot (starts at 1)
    bool active = false;     // Whether this slot currently holds a pipeline
  };

  vk::Device mDevice;
  ShaderLibrary& mShaders;
  vk::UniquePipelineCache mPipelineCache;

  std::vector<PipelineEntry> mTrackedPipelines;
  std::vector<uint32_t> mFreeSlots; // Indices of inactive slots for reuse
  std::vector<vk::UniquePipelineLayout> mCreatedLayouts;
};

} // namespace SD

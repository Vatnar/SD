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

namespace sd {

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
  
  static constexpr Handle null_handle{0, 0};

  explicit PipelineFactory(VkDevice device, ShaderLibrary& shaders);
  ~PipelineFactory();

  struct PipelineDesc {
    std::string vert_path, frag_path;
    VkRenderPass render_pass = VK_NULL_HANDLE;
    u32 subpass = 0;
    VkPolygonMode polygon_mode = VK_POLYGON_MODE_FILL;

    ~PipelineDesc() = default;
    auto operator<=>(const PipelineDesc&) const = default;
  };

  /// Create a graphics pipeline and return a stable handle.
  /// The handle remains valid even after RecreateAllPipelines() is called.
  Handle create_graphics_pipeline(const PipelineDesc& desc, VkPipelineLayout layout);
  
  /// Get the current VkPipeline for a handle.
  /// Returns VK_NULL_HANDLE if handle is invalid or pipeline was destroyed.
  VkPipeline get_pipeline(Handle handle) const;
  
  /// Check if a handle is valid (exists and generation matches).
  bool is_valid(Handle handle) const;
  
  /// Destroy a pipeline and invalidate its handle.
  /// Subsequent GetPipeline() calls will return VK_NULL_HANDLE.
  void destroy_pipeline(Handle handle);

  VkPipelineLayout create_push_constant_layout(VkShaderStageFlags stages, u32 size);

  /// Recreates all tracked pipelines.
  /// Usually called after clearing ShaderLibrary cache.
  /// Handles remain valid - they resolve to the new pipeline instances.
  /// @return Vector of failed shader path pairs (vert, frag) for any pipelines that failed to recreate.
  std::vector<std::pair<std::string, std::string>> recreate_all_pipelines();

  PipelineFactory(const PipelineFactory&) = delete;
  PipelineFactory& operator=(const PipelineFactory&) = delete;

private:
  VkResult create_pipeline_instance(const PipelineDesc& desc, vk::PipelineLayout layout,
                                  vk::UniquePipeline* pipeline);

  struct PipelineEntry {
    vk::UniquePipeline pipeline;
    PipelineDesc desc;
    vk::PipelineLayout layout;
    uint32_t generation = 1; // Generation for this slot (starts at 1)
    bool active = false;     // Whether this slot currently holds a pipeline
  };

  vk::Device m_device;
  ShaderLibrary& m_shaders;
  vk::UniquePipelineCache m_pipeline_cache;

  std::vector<PipelineEntry> m_tracked_pipelines;
  std::vector<uint32_t> m_free_slots; // Indices of inactive slots for reuse
  std::vector<vk::UniquePipelineLayout> m_created_layouts;
};

} // namespace SD

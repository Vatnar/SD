// TODO(docs): Add file-level Doxygen header
//   - @file ShaderLibrary.hpp
//   - @brief HLSL to SPIR-V compilation and shader module caching
//   - Hot reload support via file change detection
#pragma once

#include "Core/Base.hpp"
#include "Core/ShaderCompiler.hpp"
#include "VulkanConfig.hpp"

#include <string>
#include <unordered_map>
#include <vector>
#include <filesystem>
#include <set>

namespace SD {

// TODO(docs): Document ShaderLibrary class
//   - Purpose: Compiles HLSL to SPIR-V and caches VkShaderModules
//   - Load() method usage and profile parameter
//   - Hot reload: CheckForChanges() + ClearCache() pattern
//   - Integration with PipelineFactory
//   - Example: Loading and using shaders
/// Compiles HLSL → SPIR-V and caches VkShaderModules.
class ShaderLibrary {
public:
  explicit ShaderLibrary(VkDevice device);
  ~ShaderLibrary();

  VkShaderModule Load(const std::string& hlslPath, const std::string& profile);
  
  /// Checks if any cached shaders have changed on disk.
  /// Returns a set of paths that have been modified.
  std::set<std::string> CheckForChanges();

  /// Destroys all shader modules and clears the cache.
  void ClearCache();

  ShaderLibrary(const ShaderLibrary&) = delete;
  ShaderLibrary& operator=(const ShaderLibrary&) = delete;

private:
  struct ShaderEntry {
    vk::UniqueShaderModule module;
    std::filesystem::file_time_type lastWriteTime;
    std::string profile;
  };

  VkDevice mDevice;
  std::unordered_map<std::string, ShaderEntry> mCache;
  ShaderCompiler mCompiler;
};

} // namespace SD

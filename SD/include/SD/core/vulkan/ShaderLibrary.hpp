// TODO(docs): Add file-level Doxygen header
//   - @file ShaderLibrary.hpp
//   - @brief HLSL to SPIR-V compilation and shader module caching
//   - Hot reload support via file change detection
#pragma once

#include <filesystem>
#include <set>
#include <string>
#include <unordered_map>

#include <vulkan/vulkan.hpp>

#include "SD/core/ShaderCompiler.hpp"
namespace sd {

// TODO(docs): Document ShaderLibrary class
//   - Purpose: Compiles HLSL to SPIR-V and caches VkShaderModules
//   - Load() method usage and profile parameter
//   - Hot reload: CheckForChanges() + ClearCache() pattern
//   - Example: Loading and using shaders
/// Compiles HLSL → SPIR-V and caches VkShaderModules.
class ShaderLibrary {
public:
  explicit ShaderLibrary(VkDevice device);
  ~ShaderLibrary();

  VkShaderModule load(const std::string& hlsl_path, const std::string& profile);

  /// Checks if any cached shaders have changed on disk.
  /// Returns a set of paths that have been modified.
  std::set<std::string> check_for_changes();

  /// Destroys all shader modules and clears the cache.
  void clear_cache();

  ShaderLibrary(const ShaderLibrary&)            = delete;
  ShaderLibrary& operator=(const ShaderLibrary&) = delete;

private:
  struct ShaderEntry {
    vk::UniqueShaderModule          module;
    std::filesystem::file_time_type last_write_time;
    std::string                     profile;
  };

  VkDevice                                     m_device;
  std::unordered_map<std::string, ShaderEntry> m_cache;
  ShaderCompiler                               m_compiler;
};

} // namespace sd

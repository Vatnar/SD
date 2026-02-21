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

#include "Core/Vulkan/ShaderLibrary.hpp"

#include "Core/Logging.hpp"

namespace SD {

ShaderLibrary::ShaderLibrary(VkDevice device) : mDevice(device) {
}

ShaderLibrary::~ShaderLibrary() = default;

VkShaderModule ShaderLibrary::Load(const std::string& hlslPath, const std::string& profile) {
  auto it = mCache.find(hlslPath);
  if (it != mCache.end())
    return it->second.module.get();

  std::string actualPath = hlslPath;
  if (!std::filesystem::exists(actualPath)) {
    actualPath = "../" + hlslPath;
  }
  if (!std::filesystem::exists(actualPath)) {
    actualPath = "../../SDEngine/" + hlslPath;
  }

  std::vector<uint32_t> spv;
  if (!mCompiler.CompileShader(actualPath, spv, profile))
    Abort("Shader compile fail: " + hlslPath);

  vk::ShaderModuleCreateInfo createInfo{{}, spv.size() * sizeof(uint32_t), spv.data()};

  vk::UniqueShaderModule module =
      CheckVulkanResVal(vk::Device(mDevice).createShaderModuleUnique(createInfo),
                        "Failed to create shader module: " + hlslPath);


  VkShaderModule rawModule = module.get();
  ShaderEntry entry;
  entry.module = std::move(module);
  try {
    entry.lastWriteTime = std::filesystem::last_write_time(hlslPath);
  } catch (...) {
    entry.lastWriteTime = std::filesystem::file_time_type::min();
  }
  entry.profile = profile;

  mCache[hlslPath] = std::move(entry);
  return rawModule;
}

std::set<std::string> ShaderLibrary::CheckForChanges() {
  std::set<std::string> changed;
  for (auto& [path, entry] : mCache) {
    try {
      auto currentTime = std::filesystem::last_write_time(path);
      if (currentTime > entry.lastWriteTime) {
        changed.insert(path);
      }
    } catch (...) {
      // File might be temporarily locked or deleted, skip for now
    }
  }
  return changed;
}

void ShaderLibrary::ClearCache() {
  mCache.clear();
}

} // namespace SD

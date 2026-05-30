#include "core/vulkan/ShaderLibrary.hpp"

#include <system_error>

namespace sd {

ShaderLibrary::ShaderLibrary(VkDevice device) : m_device(device) {
}

ShaderLibrary::~ShaderLibrary() = default;

VkShaderModule ShaderLibrary::load(const std::string& hlsl_path, const std::string& profile) {
  auto it = m_cache.find(hlsl_path);
  if (it != m_cache.end())
    return it->second.module.get();

  std::string actual_path = hlsl_path;
  if (!std::filesystem::exists(actual_path)) {
    actual_path = "../" + hlsl_path;
  }
  if (!std::filesystem::exists(actual_path)) {
    actual_path = "../../SDEngine/" + hlsl_path;
  }

  std::vector<u32> spv;
  if (!m_compiler.compile_shader(actual_path, spv, profile))
    engine_abort("Shader compile fail: " + hlsl_path);

  vk::ShaderModuleCreateInfo create_info{{}, spv.size() * sizeof(uint32_t), spv.data()};

  vk::UniqueShaderModule module =
      check_vulkan_res_val(vk::Device(m_device).createShaderModuleUnique(create_info),
                        "Failed to create shader module: " + hlsl_path);


  VkShaderModule raw_module = module.get();
  ShaderEntry entry;
  entry.module = std::move(module);
  std::error_code ec;
  entry.last_write_time = std::filesystem::last_write_time(hlsl_path, ec);
  if (ec) {
    entry.last_write_time = std::filesystem::file_time_type::min();
  }
  entry.profile = profile;

  m_cache[hlsl_path] = std::move(entry);
  return raw_module;
}

std::set<std::string> ShaderLibrary::check_for_changes() {
  std::set<std::string> changed;
  for (auto& [path, entry] : m_cache) {
    std::error_code ec;
    auto current_time = std::filesystem::last_write_time(path, ec);
    if (!ec && current_time > entry.last_write_time) {
      changed.insert(path);
    }
  }
  return changed;
}

void ShaderLibrary::ClearCache() {
  m_cache.clear();
}

} // namespace SD

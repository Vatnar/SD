#pragma once
#include <optional>
#include <string>
#include <vector>

#include "SD/export.hpp"
#include "types.hpp"
namespace sd {
/**
 * @param filename input hlsl source
 * @param profile Optional override: "vs_6_0", "ps_6_0", "cs_6_0" etc.
 *   When omitted, the profile is deduced from the file extension.
 *
 *   Check:
 * https://docs.vulkan.org/guide/latest/hlsl.html#_vulkan_shader_stage_to_hlsl_target_shader_profile_mapping
 * for what shader stage is needed for the different HLSL target shader profiles
 */
SD_EXPORT std::optional<std::vector<u32>> compile_shader(const std::string& filename,
                                                         const std::string& profile = {});
} // namespace sd

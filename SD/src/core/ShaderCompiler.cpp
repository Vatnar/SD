#include "SD/core/ShaderCompiler.hpp"

// DXC
#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>
#include <vector>

#include <dxc/dxcapi.h>

#include "SD/core/logging.hpp"
#include "SD/utils/FixedString.hpp"
#include "SD/utils/utils.hpp"
#ifdef _WIN32
#include <wrl/client.h>
using Microsoft::WRL::ComPtr;
#else
#include <dxc/WinAdapter.h>

// NOLINTBEGIN(readability-identifier-naming)
namespace Microsoft::WRL {
template<typename T>
using ComPtr = CComPtr<T>;
}
using Microsoft::WRL::ComPtr;
// NOLINTEND(readability-identifier-naming)
#endif


namespace {
constexpr const char* k_shader_model = "6_0";


struct ExtMapping {
  std::string_view ext;
  std::string_view profile;
};

// Sorted lexicographically by ext for binary search in deduce_profile.
constexpr std::array k_ext_to_profile = {
    ExtMapping{   "ahit", "lib"},
    ExtMapping{    "amp",  "as"},
    ExtMapping{ "anyhit", "lib"},
    ExtMapping{     "as",  "as"},
    ExtMapping{   "comp",  "cs"},
    ExtMapping{"compute",  "cs"},
    ExtMapping{     "cs",  "cs"},
    ExtMapping{     "ds",  "ds"},
    ExtMapping{   "frag",  "ps"},
    ExtMapping{     "fs",  "ps"},
    ExtMapping{   "geom",  "gs"},
    ExtMapping{     "gs",  "gs"},
    ExtMapping{     "hs",  "hs"},
    ExtMapping{    "lib", "lib"},
    ExtMapping{   "mesh",  "ms"},
    ExtMapping{     "ms",  "ms"},
    ExtMapping{  "pixel",  "ps"},
    ExtMapping{     "ps",  "ps"},
    ExtMapping{  "rahit", "lib"},
    ExtMapping{  "rcall", "lib"},
    ExtMapping{  "rchit", "lib"},
    ExtMapping{   "rgen", "lib"},
    ExtMapping{   "rint", "lib"},
    ExtMapping{  "rmiss", "lib"},
    ExtMapping{   "task",  "as"},
    ExtMapping{   "tesc",  "hs"},
    ExtMapping{   "tese",  "ds"},
    ExtMapping{   "vert",  "vs"},
    ExtMapping{     "vs",  "vs"},
};

// TODO: compute max length
using ExtString = FixedString<20>; // slightly overkill, but doesnt matter
template<usize N>
consteval std::array<ExtString, N> concatenate_mappings(std::array<ExtMapping, N> mappings) {
  std::array<ExtString, N> result;

  for (usize i = 0; i < mappings.size(); ++i) {
    result[i] = ExtString{mappings[i].ext} + ExtString{':'} + ExtString{mappings[i].profile};
  }

  return result;
}

template<usize N>
constexpr std::array<ExtMapping, N> sort_by_profile(std::array<ExtMapping, N> mappings) {
  for (usize i = 0; i < N; ++i)
    for (usize j = i + 1; j < N; ++j)
      if (mappings[i].profile > mappings[j].profile ||
          (mappings[i].profile == mappings[j].profile && mappings[i].ext > mappings[j].ext))
        std::swap(mappings[i], mappings[j]);
  return mappings;
}

constexpr auto k_display_mappings = sort_by_profile(k_ext_to_profile);
constexpr std::array<ExtString, k_display_mappings.size()> k_display_exts =
    concatenate_mappings(k_display_mappings);

// Only supports ASCII
std::wstring to_wstring(const std::string& str) {
  return {str.begin(), str.end()};
}

ComPtr<IDxcUtils> init_dxc_utils() {
  ComPtr<IDxcUtils> utils;
  if (FAILED(DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&utils))))
    sd::log::engine::critical("Failed to create DXC utils instance");
  return utils;
}

ComPtr<IDxcCompiler3> init_dxc_compiler() {
  ComPtr<IDxcCompiler3> compiler;
  if (FAILED(DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&compiler))))
    sd::log::engine::critical("Failed to create DXC Compiler instance");
  return compiler;
}

std::string deduce_profile(const std::string& filename) {
  auto dot = filename.rfind('.');
  if (dot == std::string::npos) {
    sd::log::engine::shader::warn("No extension in '{}', falling back to lib_6_9", filename);
    return std::string{"lib_"} + k_shader_model;
  }

  std::string ext = filename.substr(dot + 1);
  for (auto& c : ext)
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

  auto it = std::lower_bound(
      k_ext_to_profile.begin(),
      k_ext_to_profile.end(),
      ext,
      [](const ExtMapping& mapping, const std::string& key) { return mapping.ext < key; });
  if (it != k_ext_to_profile.end() && it->ext == ext) {
    return std::string(it->profile) + '_' + k_shader_model;
  }
  std::string fallback_profile = std::string{"lib_"} + k_shader_model;
  sd::log::engine::shader::error("Unknown file extension {} for HLSL shader, falling back to "
                                 "{}\n The supported extensions are:\n{}",
                                 ext,
                                 fallback_profile,
                                 sd::tab_format(k_display_exts, 5));

  return std::string{"lib_"} + k_shader_model;
}
} // namespace

namespace sd {
std::optional<std::vector<u32>> compile_shader(const std::string& filename,
                                               const std::string& profile) {
  static ComPtr<IDxcUtils>     s_utils    = init_dxc_utils();
  static ComPtr<IDxcCompiler3> s_compiler = init_dxc_compiler();

  ComPtr<IDxcBlobEncoding> source_blob;
  std::wstring             w_filename = to_wstring(filename);

  HRESULT hres = s_utils->LoadFile(w_filename.c_str(), nullptr, &source_blob);
  if (FAILED(hres)) {
    log::engine::shader::error("Failed to load file {}", filename);
    return {};
  }

  std::string  target_profile   = profile.empty() ? deduce_profile(filename) : profile;
  std::wstring w_target_profile = to_wstring(target_profile);

  std::vector arguments = {
      L"-E",
      L"main",
      L"-T",
      w_target_profile.c_str(),
      L"-spirv",
      L"-fvk-use-dx-layout",
  };

#if SD_DEBUG
  arguments.push_back(L"-D_DEBUG");
  arguments.push_back(L"-Zi");
  arguments.push_back(L"-Qembed_debug");
#endif

  DxcBuffer buffer{.Ptr      = source_blob->GetBufferPointer(),
                   .Size     = source_blob->GetBufferSize(),
                   .Encoding = DXC_CP_ACP};

  ComPtr<IDxcResult> result;
  hres = s_compiler->Compile(&buffer,
                             arguments.data(),
                             static_cast<u32>(arguments.size()),
                             nullptr,
                             IID_PPV_ARGS(&result));
  if (SUCCEEDED(hres))
    result->GetStatus(&hres);

  if (FAILED(hres) && result) {
    ComPtr<IDxcBlobEncoding> error_blob;
    if (SUCCEEDED(result->GetErrorBuffer(&error_blob)) && error_blob) {
      log::engine::shader::error("Shader compilation failed:\n{}",
                                 static_cast<const char*>(error_blob->GetBufferPointer()));
    }
    return {};
  }

  ComPtr<IDxcBlob> code;
  result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&code), nullptr);

  auto* ptr   = static_cast<const u32*>(code->GetBufferPointer());
  auto  count = code->GetBufferSize() / sizeof(u32);
  return std::vector<u32>(ptr, ptr + count);
}
} // namespace sd

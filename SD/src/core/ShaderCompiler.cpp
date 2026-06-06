#include "SD/core/ShaderCompiler.hpp"

// DXC
#include <dxc/dxcapi.h>

#include "SD/core/logging.hpp"

#ifdef _WIN32
#include <wrl/client.h>
using Microsoft::WRL::ComPtr;
#else
#include <dxc/WinAdapter.h>
#endif

namespace {
constexpr const char* k_shader_model = "6_9";

const std::unordered_map<std::string, std::string> k_ext_to_profile = {
    { "vert",  "vs"},
    {   "vs",  "vs"},
    { "frag",  "fs"},
    {   "ps",  "fs"},
    {   "fs",  "fs"},
    { "comp",  "cs"},
    {   "cs",  "cs"},
    { "geom",  "gs"},
    {   "gs",  "gs"},
    { "tesc",  "hs"},
    {   "hs",  "hs"},
    { "tese",  "ds"},
    {   "ds",  "ds"},
    { "mesh",  "ms"},
    {   "ms",  "ms"},
    { "task",  "as"},
    {   "as",  "as"},
    { "rgen", "lib"},
    {"rmiss", "lib"},
    {"rchit", "lib"},
    {"rahit", "lib"},
    {"rcall", "lib"},
    {  "lib", "lib"},
};

CComPtr<IDxcUtils> init_dxc_utils() {
  CComPtr<IDxcUtils> utils;
  if (FAILED(DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&utils))))
    sd::log::engine::critical("Failed to create DXC utils instance");
  return utils;
}

CComPtr<IDxcCompiler3> init_dxc_compiler() {
  CComPtr<IDxcCompiler3> compiler;
  if (FAILED(DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&compiler))))
    sd::log::engine::critical("Failed to create DXC Compiler instance");
  return compiler;
}

std::string deduce_profile(const std::string& filename) {
  auto dot = filename.rfind('.');
  if (dot == std::string::npos) {
    sd::log::engine::shader::warn("No extension in '{}', falling back to lib_6_9", filename);
    return std::string{"lib_6_9"};
  }

  std::string ext = filename.substr(dot + 1);
  // Normalise to lowercase
  for (auto& c : ext)
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

  auto it = k_ext_to_profile.find(ext);
  if (it == k_ext_to_profile.end()) {
    sd::log::engine::shader::warn("Unknown extension '{}' in '{}', falling back to lib_6_9",
                                  ext,
                                  filename);
    return std::string{"lib_6_9"};
  }

  return it->second + '_' + k_shader_model;
}
} // namespace

namespace sd {
std::optional<std::vector<u32>> compile_shader(const std::string& filename,
                                               const std::string& profile) {
  local_persist CComPtr<IDxcUtils> s_utils        = init_dxc_utils();
  local_persist CComPtr<IDxcCompiler3> s_compiler = init_dxc_compiler();

  CComPtr<IDxcBlobEncoding> source_blob;
  HRESULT                   hres = s_utils->LoadFile(CA2W(filename.c_str()), nullptr, &source_blob);
  if (FAILED(hres)) {
    log::engine::shader::error("Failed to load file {}", filename);
    return {};
  }

  std::string target_profile = profile.empty() ? deduce_profile(filename) : profile;

  std::vector<LPCWSTR> arguments = {
      CA2W(filename.c_str()),
      L"-E",
      L"main",
      L"-T",
      CA2W(target_profile.c_str()),
      L"-spirv",
  };
#if SD_DEBUG
  arguments.push_back(L"-D_DEBUG");
  arguments.push_back(L"-Zi");
  arguments.push_back(L"-Qembed_debug");
#endif

  DxcBuffer buffer{.Ptr      = source_blob->GetBufferPointer(),
                   .Size     = source_blob->GetBufferSize(),
                   .Encoding = DXC_CP_ACP};

  CComPtr<IDxcResult> result;
  hres = s_compiler->Compile(&buffer,
                             arguments.data(),
                             static_cast<u32>(arguments.size()),
                             nullptr,
                             IID_PPV_ARGS(&result));
  if (SUCCEEDED(hres))
    result->GetStatus(&hres);

  if (FAILED(hres) && result) {
    CComPtr<IDxcBlobEncoding> error_blob;
    if (SUCCEEDED(result->GetErrorBuffer(&error_blob)) && error_blob) {
      log::engine::shader::error("Shader compilation failed:\n{}",
                                 static_cast<const char*>(error_blob->GetBufferPointer()));
    }
    return {};
  }

  CComPtr<IDxcBlob> code;
  result->GetResult(&code);

  auto* ptr   = static_cast<const u32*>(code->GetBufferPointer());
  auto  count = code->GetBufferSize() / sizeof(u32);
  return std::vector<u32>(ptr, ptr + count);
}
} // namespace sd

#pragma once

// DXC
#include <dxc/dxcapi.h>

#include "Core/Base.hpp"
#include "Utils/FileUtils.hpp"

#ifdef _WIN32
#include <wrl/client.h>
using Microsoft::WRL::ComPtr;
#else
#include <dxc/WinAdapter.h>
#endif

namespace sd {
class ShaderCompiler {
public:
  ShaderCompiler() {
    if (FAILED(DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&dxc_utils))))
      engine_abort("Failed to create DXC Utils");
    if (FAILED(DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&dxc_compiler))))
      engine_abort("Failed to create DXC Compiler");
  }

  // TODO: Add caching mechanism to avoid recompiling shaders if source hasn't changed
  bool compile_shader(const std::string& source, std::vector<uint32_t>& output,
                     const std::string& profile) const {
    if (!dxc_utils || !dxc_compiler) {
      log::engine::error("Shader compiler not initialised");
      return false;
    }

    CComPtr<IDxcBlobEncoding> p_source;
    if (FAILED(dxc_utils->LoadFile(CA2W(source.c_str()), nullptr, &p_source))) {
      log::engine::error("Failed to load shader source: {}", source);
      return false;
    }

    DxcBuffer dxc_buffer{};
    dxc_buffer.Ptr = p_source->GetBufferPointer();
    dxc_buffer.Size = p_source->GetBufferSize();
    BOOL known = FALSE;
    UINT32 code_page = 0;
    if (SUCCEEDED(p_source->GetEncoding(&known, &code_page))) {
      dxc_buffer.Encoding = known ? code_page : DXC_CP_ACP;
    } else {
      dxc_buffer.Encoding = DXC_CP_ACP;
    }

    std::vector<std::wstring> args;
    args.emplace_back(L"-E");
    args.emplace_back(L"main");
    args.emplace_back(L"-T");
    args.emplace_back(CA2W(profile.c_str()));
    args.emplace_back(L"-spirv");

#if _DEBUG
    args.emplace_back(L"-D_DEBUG");
    args.emplace_back(L"-Zi");
    args.emplace_back(L"-Qembed_debug");
#endif

    std::vector<LPCWSTR> psz_args;
    psz_args.reserve(args.size());
    for (const auto& arg : args)
      psz_args.push_back(arg.c_str());

    CComPtr<IDxcResult> dxc_result;
    if (FAILED(dxc_compiler->Compile(&dxc_buffer, psz_args.data(), psz_args.size(), nullptr,
                                    IID_PPV_ARGS(&dxc_result)))) {
      log::engine::error("Failed to compile shader");
      return false;
    }

    CComPtr<IDxcBlobUtf8> dxc_blob_utf8 = nullptr;
    dxc_result->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&dxc_blob_utf8), nullptr);
    if (dxc_blob_utf8 != nullptr && dxc_blob_utf8->GetStringLength() != 0) {
      log::engine::error("Shader compilation errors/warnings:\n{}", dxc_blob_utf8->GetStringPointer());
    }

    HRESULT hr_status;
    dxc_result->GetStatus(&hr_status);
    if (FAILED(hr_status)) {
      log::engine::error("Shader compilation failed");
      return false;
    }

    CComPtr<IDxcBlob> dxc_blob = nullptr;
    if (FAILED(dxc_result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&dxc_blob), nullptr))) {
      return false;
    }

    output.resize((dxc_blob->GetBufferSize() + 3) / 4);
    memcpy(output.data(), dxc_blob->GetBufferPointer(), dxc_blob->GetBufferSize());

    return true;
  }

private:
  CComPtr<IDxcUtils> dxc_utils;
  CComPtr<IDxcCompiler3> dxc_compiler;
};
inline CComPtr<IDxcUtils> g_dxc_utils;
inline CComPtr<IDxcCompiler3> g_dxc_compiler;
} // namespace SD

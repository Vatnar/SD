#pragma once

// DXC
#include <dxc/dxcapi.h>
#include <iostream>

#include "Utils/FileUtils.hpp"
#include "Utils/Utils.hpp"

#ifdef _WIN32
#include <wrl/client.h>
using Microsoft::WRL::ComPtr;
#else
#include <dxc/WinAdapter.h>
#endif

class ShaderCompiler {
public:
  ShaderCompiler() {
    if (FAILED(DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&dxcUtils))))
      Engine::Abort("Failed to create DXC Utils");
    if (FAILED(DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&dxcCompiler))))
      Engine::Abort("Failed to create DXC Compiler");
  }

  // TODO: Add caching mechanism to avoid recompiling shaders if source hasn't changed
  bool CompileShader(const std::string& source, std::vector<char>& output,
                     const std::string& profile) const {
    if (!dxcUtils || !dxcCompiler) {
      std::cerr << "Shader compiler not initialised" << std::endl;
      return false;
    }

    CComPtr<IDxcBlobEncoding> pSource;
    if (FAILED(dxcUtils->LoadFile(CA2W(source.c_str()), nullptr, &pSource))) {
      std::cerr << "Failed to load shader source: " << source << std::endl;
      return false;
    }

    DxcBuffer Source{};
    Source.Ptr = pSource->GetBufferPointer();
    Source.Size = pSource->GetBufferSize();
    BOOL known = FALSE;
    UINT32 codePage = 0;
    if (SUCCEEDED(pSource->GetEncoding(&known, &codePage))) {
      Source.Encoding = known ? codePage : DXC_CP_ACP;
    } else {
      Source.Encoding = DXC_CP_ACP;
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

    std::vector<LPCWSTR> pszArgs;
    pszArgs.reserve(args.size());
    for (const auto& arg : args)
      pszArgs.push_back(arg.c_str());

    CComPtr<IDxcResult> pResults;
    if (FAILED(dxcCompiler->Compile(&Source, pszArgs.data(), pszArgs.size(), nullptr,
                                    IID_PPV_ARGS(&pResults)))) {
      std::cerr << "Failed to compile shader" << std::endl;
      return false;
    }

    CComPtr<IDxcBlobUtf8> pErrors = nullptr;
    pResults->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&pErrors), nullptr);
    if (pErrors != nullptr && pErrors->GetStringLength() != 0) {
      std::cerr << "Shader compilation errors/warnings:\n"
                << pErrors->GetStringPointer() << std::endl;
    }

    HRESULT hrStatus;
    pResults->GetStatus(&hrStatus);
    if (FAILED(hrStatus)) {
      std::cerr << "Shader compilation failed." << std::endl;
      return false;
    }

    CComPtr<IDxcBlob> pShader = nullptr;
    if (FAILED(pResults->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&pShader), nullptr))) {
      return false;
    }

    output.resize(pShader->GetBufferSize());
    memcpy(output.data(), pShader->GetBufferPointer(), pShader->GetBufferSize());

    return true;
  }

private:
  CComPtr<IDxcUtils> dxcUtils;
  CComPtr<IDxcCompiler3> dxcCompiler;
};
inline CComPtr<IDxcUtils> g_dxcUtils;
inline CComPtr<IDxcCompiler3> g_dxcCompiler;

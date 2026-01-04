#pragma once
#include "VulkanConfig.hpp"
#include "stb_image.h"
#include "nvrhi/nvrhi.h"
#include "spdlog/spdlog.h"

#include <expected>
#include <filesystem>
#include <iostream>

namespace Engine
{

    [[noreturn]] inline void Abort(const std::string& message)
    {
        if (const auto logger = spdlog::get("engine"))
        {
            logger->critical("Fatal error: {}", message);
            spdlog::shutdown();
        }
        else
        {
            std::cerr << "Fatal error: " << message << '\n';
        }
        std::exit(EXIT_FAILURE);
    }
} // namespace Engine


inline nvrhi::Format toNvrhiFormat(const vk::Format fmt)
{
    switch (fmt)
    {
        case vk::Format::eB8G8R8A8Srgb:
            return nvrhi::Format::SBGRA8_UNORM;
        case vk::Format::eR8G8B8A8Srgb:
            return nvrhi::Format::SRGBA8_UNORM;
        case vk::Format::eB8G8R8A8Unorm:
            return nvrhi::Format::BGRA8_UNORM;
        case vk::Format::eR8G8B8A8Unorm:
            return nvrhi::Format::RGBA8_UNORM;
        default:
            Engine::Abort("Unsupported device format");
    }
}


enum class TextureError
{
    FailedToLoad
};
inline std::expected<nvrhi::TextureHandle, TextureError> CreateTexture(const nvrhi::DeviceHandle&   nvrhiDevice,
                                                                       const std::filesystem::path& filePath)
{
    auto logger = spdlog::get("engine");

    auto     cmdList = nvrhiDevice->createCommandList();
    int      texWidth, texHeight, texChannels;
    stbi_uc *texPixels = stbi_load(filePath.c_str(), &texWidth, &texHeight, &texChannels, 4);

    if (!texPixels)
    {
        return std::unexpected(TextureError::FailedToLoad);
    }

    nvrhi::TextureDesc textureDesc;
    textureDesc.width            = texWidth;
    textureDesc.height           = texHeight;
    textureDesc.format           = nvrhi::Format::RGBA8_UNORM;
    textureDesc.debugName        = filePath.string();
    textureDesc.initialState     = nvrhi::ResourceStates::ShaderResource;
    textureDesc.keepInitialState = true;
    textureDesc.dimension        = nvrhi::TextureDimension::Texture2D;

    auto texture = nvrhiDevice->createTexture(textureDesc);

    cmdList->open();
    cmdList->writeTexture(texture, 0, 0, texPixels, texWidth * 4);
    cmdList->close();
    nvrhiDevice->executeCommandList(cmdList);
    stbi_image_free(texPixels);
    return texture;
}

inline nvrhi::TextureHandle GetDefaultWhiteTexture(const nvrhi::DeviceHandle& nvrhiDevice)
{
    auto               cmdList = nvrhiDevice->createCommandList();
    nvrhi::TextureDesc textureDesc;
    textureDesc.width            = 1;
    textureDesc.height           = 1;
    textureDesc.format           = nvrhi::Format::RGBA8_UNORM;
    textureDesc.debugName        = "FailedTexture";
    textureDesc.initialState     = nvrhi::ResourceStates::ShaderResource;
    textureDesc.keepInitialState = true;
    auto     texture             = nvrhiDevice->createTexture(textureDesc);
    uint32_t white               = 0xFFFFFFFF;

    cmdList->open();
    cmdList->writeTexture(texture, 0, 0, &white, 4);
    cmdList->close();
    nvrhiDevice->executeCommandList(cmdList);
    return texture;
}

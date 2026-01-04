// spdlog
#include <spdlog/spdlog.h>
#include <spdlog/async.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>


// VULKAN
#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#define VULKAN_HPP_ENABLE_DYNAMIC_LOADER_TOOL 1
#include <vulkan/vulkan.hpp>
VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

// NVRHI
#include <nvrhi/vulkan.h>


#include <nvrhi/utils.h>
#include <nvrhi/validation.h>

// GLFW
#include <GLFW/glfw3.h>

// STB
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

// STD
#include <iostream>
#include <queue>
#include <chrono>
#include <x86intrin.h>
#include <filesystem>
#include <fstream>

// SD
// #include "assets/shaders/vertex_shader.h"
// #include "assets/shaders/pixel_shader.h"


constexpr bool enableValidation     = true;
constexpr int  MAX_FRAMES_IN_FLIGHT = 2;


#include <dxc/dxcapi.h>
// extern unsigned char vertex_spv[], pixel_spv[];
// extern unsigned int  vertex_spv_len, pixel_spv_len;

struct Vertex
{
    float position[3];
    float texCoord[2];
};

nvrhi::Format toNvrhiFormat(vk::Format fmt)
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
            throw std::runtime_error("Unsupported");
    }
}


struct MessageCallback : public nvrhi::IMessageCallback
{
    void message(nvrhi::MessageSeverity severity, const char *messageText) override;
};

void MessageCallback::message(nvrhi::MessageSeverity severity, const char *messageText)
{
    switch (severity)
    {
        using nvrhi::MessageSeverity;

        case MessageSeverity::Info:
            std::cout << "nvrhi::INFO: " << messageText << std::endl;
            break;
        case MessageSeverity::Warning:
            std::cout << "nvrhi::WARNING: " << messageText << std::endl;
            break;
        case MessageSeverity::Error:
            std::cerr << "nvrhi::ERROR: " << messageText << std::endl;
            break;
        case MessageSeverity::Fatal:
            std::cerr << "nvrhi::FATAL: " << messageText << std::endl;
            break;
        default:
            std::cout << "nvrhi::OTHER " << messageText << std::endl;
    }
}

MessageCallback  g_MessageCallback;
VkPhysicalDevice vulkanPhysicalDevice = nullptr;


#define ENGINE_LOG_LEVEL_TRACE 0
#define ENGINE_LOG_LEVEL_DEBUG 1
#define ENGINE_LOG_LEVEL_INFO 2
#define ENGINE_LOG_LEVEL_WARN 3
#define ENGINE_LOG_LEVEL_ERROR 4
#define ENGINE_LOG_LEVEL_CRITICAL 5
#define ENGINE_LOG_LEVEL_OFF 6

enum class LogLevel
{
    Trace    = ENGINE_LOG_LEVEL_TRACE,
    Debug    = ENGINE_LOG_LEVEL_DEBUG,
    Info     = ENGINE_LOG_LEVEL_INFO,
    Warn     = ENGINE_LOG_LEVEL_WARN,
    Error    = ENGINE_LOG_LEVEL_ERROR,
    Critical = ENGINE_LOG_LEVEL_CRITICAL,
    Off      = ENGINE_LOG_LEVEL_OFF
};

#ifndef ENGINE_LOG_LEVEL
#define ENGINE_LOG_LEVEL ENGINE_LOG_LEVEL_INFO
#endif
constexpr auto cLOG_LEVEL = static_cast<LogLevel>(ENGINE_LOG_LEVEL);

bool constexpr should_log(LogLevel level)
{
    return static_cast<int>(level) >= static_cast<int>(cLOG_LEVEL);
}

void init_logging()
{
    spdlog::init_thread_pool(8192, 1);

    std::vector<spdlog::sink_ptr> sinks;
    sinks.push_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());
    sinks.push_back(std::make_shared<spdlog::sinks::basic_file_sink_mt>("engine.log", true));

    auto logger = std::make_shared<spdlog::async_logger>("engine",
                                                         sinks.begin(),
                                                         sinks.end(),
                                                         spdlog::thread_pool(),
                                                         spdlog::async_overflow_policy::block);

    switch (cLOG_LEVEL)
    {
        case LogLevel::Trace:
            logger->set_level(spdlog::level::trace);
            break;
        case LogLevel::Debug:
            logger->set_level(spdlog::level::debug);
            break;
        case LogLevel::Info:
            logger->set_level(spdlog::level::info);
            break;
        case LogLevel::Warn:
            logger->set_level(spdlog::level::warn);
            break;
        case LogLevel::Error:
            logger->set_level(spdlog::level::err);
            break;
        case LogLevel::Critical:
            logger->set_level(spdlog::level::critical);
            break;
        case LogLevel::Off:
            logger->set_level(spdlog::level::off);
            break;
        default:
            logger->set_level(spdlog::level::info);
    }
    logger->flush_on(spdlog::level::warn);

    spdlog::register_logger(logger);
}

bool g_ResizeRequested = false;

void framebufferResizeCallback(GLFWwindow *window, int width, int height)
{
    g_ResizeRequested = true;
}

std::vector<char> readFile(const std::string& filename)
{
    std::ifstream file(filename, std::ios::ate | std::ios::binary);

    if (!file.is_open())
    {
        throw std::runtime_error("failed to open file: " + filename);
    }

    size_t            fileSize = (size_t) file.tellg();
    std::vector<char> buffer(fileSize);

    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();

    return buffer;
}

CComPtr<IDxcUtils>     g_dxcUtils;
CComPtr<IDxcCompiler3> g_dxcCompiler;

bool initShaderCompiler()
{
    if (FAILED(DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&g_dxcUtils))))
    {
        std::cerr << "Failed to create DXC Utils" << std::endl;
        return false;
    }
    if (FAILED(DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&g_dxcCompiler))))
    {
        std::cerr << "Failed to create DXC Compiler" << std::endl;
        return false;
    }
    return true;
}

bool compileShader(const std::string& source, std::vector<char>& output, const std::string& profile)
{
    if (!g_dxcUtils || !g_dxcCompiler)
    {
        std::cerr << "Shader compiler not initialized" << std::endl;
        return false;
    }

    CComPtr<IDxcBlobEncoding> pSource;
    if (FAILED(g_dxcUtils->LoadFile(CA2W(source.c_str()), nullptr, &pSource)))
    {
        std::cerr << "Failed to load shader source: " << source << std::endl;
        return false;
    }

    DxcBuffer Source{};
    Source.Ptr      = pSource->GetBufferPointer();
    Source.Size     = pSource->GetBufferSize();
    BOOL   known    = FALSE;
    UINT32 codePage = 0;
    if (SUCCEEDED(pSource->GetEncoding(&known, &codePage)))
    {
        Source.Encoding = known ? codePage : DXC_CP_ACP;
    }
    else
    {
        Source.Encoding = DXC_CP_ACP;
    }

    std::vector<std::wstring> args;
    args.emplace_back(L"-E");
    args.emplace_back(L"main");
    args.emplace_back(L"-T");
    args.emplace_back(CA2W(profile.c_str()));
    args.emplace_back(L"-spirv");

    std::vector<LPCWSTR> pszArgs;
    for (const auto& arg : args)
        pszArgs.push_back(arg.c_str());

    CComPtr<IDxcResult> pResults;
    if (FAILED(g_dxcCompiler->Compile(&Source, pszArgs.data(), pszArgs.size(), nullptr, IID_PPV_ARGS(&pResults))))
    {
        std::cerr << "Failed to compile shader" << std::endl;
        return false;
    }

    CComPtr<IDxcBlobUtf8> pErrors = nullptr;
    pResults->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&pErrors), nullptr);
    if (pErrors != nullptr && pErrors->GetStringLength() != 0)
    {
        std::cerr << "Shader compilation errors/warnings:\n" << pErrors->GetStringPointer() << std::endl;
    }

    HRESULT hrStatus;
    pResults->GetStatus(&hrStatus);
    if (FAILED(hrStatus))
    {
        std::cerr << "Shader compilation failed." << std::endl;
        return false;
    }

    CComPtr<IDxcBlob> pShader = nullptr;
    if (FAILED(pResults->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&pShader), nullptr)))
    {
        return false;
    }

    output.resize(pShader->GetBufferSize());
    memcpy(output.data(), pShader->GetBufferPointer(), pShader->GetBufferSize());

    return true;
}

int main()
{
    // NOTE: Init logging

    init_logging();
    auto logger = spdlog::get("engine");

    // NOTE: Create window
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow *windowHandle = glfwCreateWindow(800, 600, "SDPrototype", nullptr, nullptr);
    glfwSetFramebufferSizeCallback(windowHandle, framebufferResizeCallback);


    // NOTE: vulkan instance
    static vk::detail::DynamicLoader dl;
    PFN_vkGetInstanceProcAddr        vkGetInstanceProcAddr =
            dl.getProcAddress<PFN_vkGetInstanceProcAddr>("vkGetInstanceProcAddr");

    VULKAN_HPP_DEFAULT_DISPATCHER.init(vkGetInstanceProcAddr);

    // Create instance
    vk::ApplicationInfo appInfo("MyEngine", 1, "NoEngine", 1, VK_API_VERSION_1_3);

    uint32_t extCount = 0;

    const char **glfwExts = glfwGetRequiredInstanceExtensions(&extCount);

    std::vector<const char *> instanceExts(glfwExts, glfwExts + extCount);


    vk::InstanceCreateInfo instInfo({}, &appInfo, {}, instanceExts);
    vk::UniqueInstance     instance = vk::createInstanceUnique(instInfo);

    VULKAN_HPP_DEFAULT_DISPATCHER.init(*instance);

    // physical device
    vk::PhysicalDevice physDev = instance->enumeratePhysicalDevices().front();

    // Create device

    vk::PhysicalDeviceVulkan12Features features12{};
    features12.timelineSemaphore   = VK_TRUE;
    features12.bufferDeviceAddress = VK_TRUE;

    vk::PhysicalDeviceVulkan13Features features13;
    features13.setDynamicRendering(true).setSynchronization2(true);

    features12.pNext = &features13;

    vk::PhysicalDeviceFeatures2 features2;
    features2.setPNext(&features12);

    auto available = physDev.enumerateDeviceExtensionProperties();

    auto supports = [&](const char *name)
    {
        return std::ranges::any_of(available,
                                   [&](const vk::ExtensionProperties& e)
                                   { return std::strcmp(e.extensionName, name) == 0; });
    };

    std::vector<const char *> deviceExts;

    auto requireExt = [&](const char *name)
    {
        if (!supports(name))
            throw std::runtime_error(std::string("Required device extension missing ") + name);
        deviceExts.push_back(name);
    };

    requireExt(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

    auto raytracing = false;
    if (raytracing)
    {
        requireExt(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME);
        requireExt(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME);
        requireExt(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME);
    }

    // NOTE: Create surface

    VkSurfaceKHR rawSurface;
    if (auto res = glfwCreateWindowSurface(*instance, windowHandle, nullptr, &rawSurface); res != VK_SUCCESS)
    {
        std::cerr << "glfwCreateWindowSurface failed with VKResult = " << res << "\n";
        throw std::runtime_error("Failed to create window surface");
    }
    vk::UniqueSurfaceKHR surface(rawSurface, *instance);


    // NOTE: setup queues

    auto     queueFamilies       = physDev.getQueueFamilyProperties();
    uint32_t graphicsFamilyIndex = UINT32_MAX;

    for (uint32_t i = 0; i < queueFamilies.size(); ++i)
    {
        bool supportsGraphics =
                (queueFamilies[i].queueFlags & vk::QueueFlagBits::eGraphics) == vk::QueueFlagBits::eGraphics;

        vk::Bool32 supportsPresent = vk::False;

        if (auto res = physDev.getSurfaceSupportKHR(i, *surface, &supportsPresent); res != vk::Result::eSuccess)
        {
            logger->warn("getSurfaceSupportKHR returned vk::Result::{}", static_cast<uint64_t>(res));
        }

        if (supportsGraphics && supportsPresent)
        {
            graphicsFamilyIndex = i;
            break;
        }
    }

    if (graphicsFamilyIndex == UINT32_MAX)
    {
        // TODO: figure out if i want exceptions, how it should be handled, where etc
        throw std::runtime_error("No queue family supports both graphics and present");
    }


    // NOTE: Create device

    float                     priority = 1.0f;
    vk::DeviceQueueCreateInfo queueInfo({}, graphicsFamilyIndex, 1, &priority);

    vk::DeviceCreateInfo devInfo({}, queueInfo, {}, deviceExts);
    devInfo.setPNext(&features2);


    vk::UniqueDevice vulkanDevice = physDev.createDeviceUnique(devInfo);

    // init dispatcher for device
    VULKAN_HPP_DEFAULT_DISPATCHER.init(*vulkanDevice);
    // endregion
    // region NVRHIDevice and validation
    // NOTE: Creating NVRHI device
    nvrhi::vulkan::DeviceDesc nvrhiDesc;
    nvrhiDesc.errorCB        = &g_MessageCallback;
    nvrhiDesc.instance       = *instance;
    nvrhiDesc.physicalDevice = physDev;
    nvrhiDesc.device         = *vulkanDevice;

    // queue
    nvrhiDesc.graphicsQueue      = vulkanDevice->getQueue(graphicsFamilyIndex, 0);
    nvrhiDesc.graphicsQueueIndex = graphicsFamilyIndex;

    // exts
    nvrhiDesc.deviceExtensions             = deviceExts.data();
    nvrhiDesc.numDeviceExtensions          = deviceExts.size();
    nvrhiDesc.bufferDeviceAddressSupported = true;

    // NVRHI Wrapper
    nvrhi::vulkan::DeviceHandle nvrhiDevice = nvrhi::vulkan::createDevice(nvrhiDesc);


    // NOTE: VALIDATION:
    if (enableValidation)
    {
        nvrhi::DeviceHandle nvrhiValidationLayer = nvrhi::validation::createValidationLayer(nvrhiDevice);
        // TODO: fix
        // nvrhiDevice                              = nvrhiValidationLayer;
    }

    // endregion


    // Find swapchain width and height

    vk::SurfaceCapabilitiesKHR caps = physDev.getSurfaceCapabilitiesKHR(*surface);
    vk::Extent2D               swapchainExtent;
    int                        windowWidth;
    int                        windowHeight;
    glfwGetWindowSize(windowHandle, &windowWidth, &windowHeight);

    if (caps.currentExtent.width != UINT32_MAX)
    {
        swapchainExtent = caps.currentExtent;
        windowWidth     = static_cast<int>(swapchainExtent.width);
        windowHeight    = static_cast<int>(swapchainExtent.height);
    }
    else
    {
        // TODO: Change to the actual window width and such, this just for testing
        swapchainExtent.width =
                std::clamp(static_cast<uint32_t>(windowWidth), caps.minImageExtent.width, caps.maxImageExtent.width);
        swapchainExtent.height =
                std::clamp(static_cast<uint32_t>(windowHeight), caps.minImageExtent.height, caps.maxImageExtent.height);
    }

    // image count
    uint32_t desiredImageCount = caps.minImageCount + 1; // try triple buffering

    if (caps.maxImageCount > 0 && desiredImageCount > caps.maxImageCount)
        desiredImageCount = caps.maxImageCount;


    // NOTE: create swapchain
    auto surfaceFormats = physDev.getSurfaceFormatsKHR(*surface);

    // get surface formats
    vk::SurfaceFormatKHR surfaceFormat;
    if (surfaceFormats.size() == 1 && surfaceFormats[0].format == vk::Format::eUndefined)
    {
        // no preference;
        // todo: study which are best
        surfaceFormat.format     = vk::Format::eB8G8R8A8Srgb;
        surfaceFormat.colorSpace = vk::ColorSpaceKHR::eSrgbNonlinear;
    }
    else
    {
        // prefer SRGB 8bit BGRA if avilable, else fallback
        surfaceFormat = surfaceFormats[0];
        for (auto& f : surfaceFormats)
        {
            if (f.format == vk::Format::eB8G8R8A8Srgb && f.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear)
            {
                surfaceFormat = f;
                break;
            }
        }
    }
    // get present mode
    auto               presentModes = physDev.getSurfacePresentModesKHR(*surface);
    vk::PresentModeKHR presentMode  = vk::PresentModeKHR::eFifo; // always supported
    for (auto m : presentModes)
    {
        if (m == vk::PresentModeKHR::eMailbox)
        {
            presentMode = m; // low-latency vsync
            break;
        }
    }

    // find image usage
    vk::ImageUsageFlags usage = vk::ImageUsageFlagBits::eColorAttachment;
    usage |= vk::ImageUsageFlagBits::eTransferDst; // for offscreen hdr buffer blitting

    // find pretransform
    // for rotated displays to work properly
    vk::SurfaceTransformFlagBitsKHR preTransform = caps.supportedTransforms & vk::SurfaceTransformFlagBitsKHR::eIdentity
                                                           ? vk::SurfaceTransformFlagBitsKHR::eIdentity
                                                           : caps.currentTransform;

    vk::SwapchainCreateInfoKHR swapchainCreateInfo;
    swapchainCreateInfo.setSurface(*surface)
            .setMinImageCount(desiredImageCount)
            .setImageFormat(surfaceFormat.format)
            .setImageColorSpace(surfaceFormat.colorSpace)
            .setImageExtent(swapchainExtent)
            .setImageArrayLayers(1)
            .setImageUsage(usage)
            .setPreTransform(preTransform)
            .setPresentMode(presentMode)
            .setClipped(true)
            .setImageSharingMode(vk::SharingMode::eExclusive); // for graphics queue == present queue

    vk::UniqueSwapchainKHR swapchain = vulkanDevice->createSwapchainKHRUnique(swapchainCreateInfo);

    // Get all swapchain images
    uint32_t imageCount = 0;

    // TODO: Check if we might aswell just put it into the vector right away, if that is better or not
    // TODO: Get/create swap chain

    if (auto res = vulkanDevice->getSwapchainImagesKHR(*swapchain, &imageCount, nullptr); res != vk::Result::eSuccess)
    {
        logger->warn("getSwapChainImagesKHR returned vk::Result::{}", static_cast<uint64_t>(res));
    }
    std::vector<vk::Image> swapchainImages(imageCount);

    if (auto res = vulkanDevice->getSwapchainImagesKHR(*swapchain, &imageCount, swapchainImages.data());
        res != vk::Result::eSuccess)
    {
        logger->warn("getSwapChainImagesKHR returned vk::Result::{}", static_cast<uint64_t>(res));
    }

    nvrhi::TextureDesc swapchainTexDesc;

    uint32_t swapchainWidth  = swapchainExtent.width;
    uint32_t swapchainHeight = swapchainExtent.height;

    // TODO: this must match surface specs
    auto swapchainTexImageFormat = toNvrhiFormat(surfaceFormat.format);

    swapchainTexDesc.setDimension(nvrhi::TextureDimension::Texture2D)
            .setWidth(swapchainWidth)
            .setHeight(swapchainHeight)
            .setFormat(swapchainTexImageFormat)
            .setIsRenderTarget(true)
            .setKeepInitialState(true)
            .setInitialState(nvrhi::ResourceStates::Present)
            .setDebugName("Swapchain Image");

    std::vector<nvrhi::TextureHandle> swapchainTextures;
    swapchainTextures.reserve(imageCount);


    // create handles for all native vk textures
    for (uint32_t i = 0; i < imageCount; ++i)
    {
        VkImage nativeImage = static_cast<VkImage>(swapchainImages[i]);

        nvrhi::TextureHandle tex = nvrhiDevice->createHandleForNativeTexture(nvrhi::ObjectTypes::VK_Image,
                                                                             nvrhi::Object(nativeImage),
                                                                             swapchainTexDesc);
        swapchainTextures.push_back(tex);
    }


    // shared depth texture for all framebuffers TODO: can we fix this in a nother way.

    nvrhi::TextureDesc depthDesc{};
    depthDesc.setDimension(nvrhi::TextureDimension::Texture2D)
            .setWidth(windowWidth)
            .setHeight(windowHeight)
            .setFormat(nvrhi::Format::D32)
            .setIsRenderTarget(true)
            .enableAutomaticStateTracking(nvrhi::ResourceStates::DepthWrite);

    nvrhi::TextureHandle depthTexture = nvrhiDevice->createTexture(depthDesc);

    // create framebuffer for each nvrhi texture
    std::vector<nvrhi::FramebufferHandle> framebuffers;
    framebuffers.reserve(imageCount);
    for (uint32_t i = 0; i < imageCount; ++i)
    {
        auto fbDesc = nvrhi::FramebufferDesc().addColorAttachment(swapchainTextures[i]);
        fbDesc.setDepthAttachment(depthTexture);
        framebuffers.push_back(nvrhiDevice->createFramebuffer(fbDesc));
    }

    nvrhi::CommandListHandle cmdList = nvrhiDevice->createCommandList();

    // Load Texture
    int      texWidth, texHeight, texChannels;
    stbi_uc *texPixels = stbi_load("assets/textures/example.jpg", &texWidth, &texHeight, &texChannels, 4);

    nvrhi::TextureHandle exampleTexture;
    if (texPixels)
    {
        nvrhi::TextureDesc textureDesc;
        textureDesc.width            = texWidth;
        textureDesc.height           = texHeight;
        textureDesc.format           = nvrhi::Format::RGBA8_UNORM;
        textureDesc.debugName        = "Example Texture";
        textureDesc.initialState     = nvrhi::ResourceStates::ShaderResource;
        textureDesc.keepInitialState = true;
        textureDesc.dimension        = nvrhi::TextureDimension::Texture2D;

        exampleTexture = nvrhiDevice->createTexture(textureDesc);

        cmdList->open();
        cmdList->writeTexture(exampleTexture, 0, 0, texPixels, texWidth * 4);
        cmdList->close();
        nvrhiDevice->executeCommandList(cmdList);

        stbi_image_free(texPixels);
    }
    else
    {
        logger->error("Failed to load texture: assets/textures/example.jpg");
        nvrhi::TextureDesc textureDesc;
        textureDesc.width            = 1;
        textureDesc.height           = 1;
        textureDesc.format           = nvrhi::Format::RGBA8_UNORM;
        textureDesc.debugName        = "Dummy Texture";
        textureDesc.initialState     = nvrhi::ResourceStates::ShaderResource;
        textureDesc.keepInitialState = true;
        exampleTexture               = nvrhiDevice->createTexture(textureDesc);
        uint32_t white               = 0xFFFFFFFF;

        cmdList->open();
        cmdList->writeTexture(exampleTexture, 0, 0, &white, 4);
        cmdList->close();
        nvrhiDevice->executeCommandList(cmdList);
    }

    // ViewProjection Buffer
    struct ViewProjection
    {
        float viewProj[16];
    };

    nvrhi::BufferDesc constantBufferDesc;
    constantBufferDesc.byteSize         = sizeof(ViewProjection);
    constantBufferDesc.isConstantBuffer = true;
    constantBufferDesc.debugName        = "ViewProjectionConstantBuffer";
    constantBufferDesc.initialState     = nvrhi::ResourceStates::ConstantBuffer;
    constantBufferDesc.keepInitialState = true;
    constantBufferDesc.cpuAccess        = nvrhi::CpuAccessMode::Write;
    nvrhi::BufferHandle viewProjBuffer  = nvrhiDevice->createBuffer(constantBufferDesc);

    // Vertex Buffer
    static const Vertex vertices[] = {
            {{-0.5f, -0.5f, 0.0f}, {0.0f, 0.0f}}, // TL
            { {0.5f, -0.5f, 0.0f}, {1.0f, 0.0f}}, // TR
            { {-0.5f, 0.5f, 0.0f}, {0.0f, 1.0f}}, // BL
            { {-0.5f, 0.5f, 0.0f}, {0.0f, 1.0f}}, // BL
            { {0.5f, -0.5f, 0.0f}, {1.0f, 0.0f}}, // TR
            {  {0.5f, 0.5f, 0.0f}, {1.0f, 1.0f}}  // BR
    };

    nvrhi::BufferDesc vertexBufferDesc;
    vertexBufferDesc.byteSize         = sizeof(vertices);
    vertexBufferDesc.isVertexBuffer   = true;
    vertexBufferDesc.debugName        = "VertexBuffer";
    vertexBufferDesc.initialState     = nvrhi::ResourceStates::VertexBuffer;
    vertexBufferDesc.keepInitialState = true;
    nvrhi::BufferHandle vertexBuffer  = nvrhiDevice->createBuffer(vertexBufferDesc);

    cmdList->open();
    cmdList->writeBuffer(vertexBuffer, vertices, sizeof(vertices));
    cmdList->close();
    nvrhiDevice->executeCommandList(cmdList);

    // Sampler
    nvrhi::SamplerDesc samplerDesc;
    samplerDesc.addressU         = nvrhi::SamplerAddressMode::Wrap;
    samplerDesc.addressV         = nvrhi::SamplerAddressMode::Wrap;
    samplerDesc.addressW         = nvrhi::SamplerAddressMode::Wrap;
    nvrhi::SamplerHandle sampler = nvrhiDevice->createSampler(samplerDesc);

    // Binding Layout
    nvrhi::BindingLayoutDesc layoutDesc;
    layoutDesc.visibility = nvrhi::ShaderType::All;
    layoutDesc.addItem(nvrhi::BindingLayoutItem::ConstantBuffer(0));
    layoutDesc.addItem(nvrhi::BindingLayoutItem::Texture_SRV(1));
    layoutDesc.addItem(nvrhi::BindingLayoutItem::Sampler(2));

    // Zero out offsets to match direct register->binding mapping
    layoutDesc.bindingOffsets = nvrhi::VulkanBindingOffsets()
                                        .setShaderResourceOffset(0)
                                        .setSamplerOffset(0)
                                        .setConstantBufferOffset(0)
                                        .setUnorderedAccessViewOffset(0);

    nvrhi::BindingLayoutHandle bindingLayout = nvrhiDevice->createBindingLayout(layoutDesc);

    // Binding Set
    nvrhi::BindingSetDesc bindingSetDesc;
    bindingSetDesc.bindings            = {nvrhi::BindingSetItem::ConstantBuffer(0, viewProjBuffer),
                                          nvrhi::BindingSetItem::Texture_SRV(1, exampleTexture),
                                          nvrhi::BindingSetItem::Sampler(2, sampler)};
    nvrhi::BindingSetHandle bindingSet = nvrhiDevice->createBindingSet(bindingSetDesc, bindingLayout);

    // NOTE: Graphics Pipeline

    // Compile shaders
    if (!initShaderCompiler())
    {
        logger->critical("Failed to initialize shader compiler");
        return -1;
    }

    std::vector<char> vertexSpv;
    if (!compileShader("assets/shaders/vertex.hlsl", vertexSpv, "vs_6_0"))
    {
        logger->critical("Failed to compile vertex shader");
        return -1;
    }

    std::vector<char> pixelSpv;
    if (!compileShader("assets/shaders/pixel.hlsl", pixelSpv, "ps_6_0"))
    {
        logger->critical("Failed to compile pixel shader");
        return -1;
    }

    nvrhi::ShaderHandle vertexShader =
            nvrhiDevice->createShader(nvrhi::ShaderDesc().setShaderType(nvrhi::ShaderType::Vertex),
                                      vertexSpv.data(),
                                      vertexSpv.size());

    nvrhi::ShaderHandle pixelShader =
            nvrhiDevice->createShader(nvrhi::ShaderDesc().setShaderType(nvrhi::ShaderType::Pixel),
                                      pixelSpv.data(),
                                      pixelSpv.size());

    if (!vertexShader || !pixelShader)
    {
        logger->critical("Failed to create shaders");
        return -1;
    }

    // vert attribs
    nvrhi::VertexAttributeDesc attributes[] = {nvrhi::VertexAttributeDesc()
                                                       .setName("POSITION")
                                                       .setFormat(nvrhi::Format::RGB32_FLOAT)
                                                       .setOffset(0)
                                                       .setElementStride(sizeof(Vertex)),
                                               nvrhi::VertexAttributeDesc()
                                                       .setName("TEXCOORD")
                                                       .setFormat(nvrhi::Format::RG32_FLOAT)
                                                       .setOffset(sizeof(float) * 3)
                                                       .setElementStride(sizeof(Vertex))};

    nvrhi::InputLayoutHandle inputLayout = nvrhiDevice->createInputLayout(attributes, 2, vertexShader);

    auto fbInfo = framebuffers[0]->getFramebufferInfo();
    logger->debug("colorFormats[0]={}", static_cast<uint32_t>(fbInfo.colorFormats[0]));

    auto renderState  = nvrhi::RenderState{};
    auto pipelineDesc = nvrhi::GraphicsPipelineDesc()
                                .setRenderState(renderState)
                                .setInputLayout(inputLayout)
                                .setVertexShader(vertexShader)
                                .setPixelShader(pixelShader)
                                .addBindingLayout(bindingLayout);

    pipelineDesc.renderState.rasterState.setCullMode(nvrhi::RasterCullMode::None);
    pipelineDesc.renderState.depthStencilState.depthTestEnable = false;
    pipelineDesc.primType                                      = nvrhi::PrimitiveType::TriangleList;

    nvrhi::GraphicsPipelineHandle graphicsPipeline = nvrhiDevice->createGraphicsPipeline(pipelineDesc, fbInfo);

    if (!graphicsPipeline)
    {
        logger->critical("Failed to create graphics pipeline");
        return -1;
    }

    // sync objects
    std::vector<vk::UniqueSemaphore> imageAcquiredSemaphores;
    std::vector<vk::UniqueSemaphore> renderCompletedSemaphores;
    std::vector<vk::UniqueFence>     frameFences;

    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
    {
        imageAcquiredSemaphores.push_back(vulkanDevice->createSemaphoreUnique({}));
        renderCompletedSemaphores.push_back(vulkanDevice->createSemaphoreUnique({}));
        frameFences.push_back(vulkanDevice->createFenceUnique({vk::FenceCreateFlagBits::eSignaled}));
    }

    bool                      isRunning    = true;
    [[maybe_unused]] uint32_t frameCount   = 0;
    uint32_t                  currentFrame = 0;

    auto     lastReportTime        = std::chrono::high_resolution_clock::now();
    uint64_t framesSinceLastReport = 0;
    uint64_t cyclesSinceLastReport = 0;

    while (isRunning)
    {
        uint64_t startCycles = __rdtsc();

        // TODO: Begin pass somehow?
        isRunning = !glfwWindowShouldClose(windowHandle);
        glfwPollEvents();

        if (g_ResizeRequested)
        {
            g_ResizeRequested = false;
            int width, height;
            glfwGetWindowSize(windowHandle, &width, &height);
            if (width > 0 && height > 0)
            {
                nvrhiDevice->waitForIdle();

                // Destroy old swapchain resources
                framebuffers.clear();
                swapchainTextures.clear();
                depthTexture = nullptr;

                // Recreate swapchain
                swapchainExtent.width  = width;
                swapchainExtent.height = height;

                swapchainCreateInfo.setImageExtent(swapchainExtent);
                swapchainCreateInfo.setOldSwapchain(*swapchain);

                vk::UniqueSwapchainKHR newSwapchain = vulkanDevice->createSwapchainKHRUnique(swapchainCreateInfo);
                swapchain                           = std::move(newSwapchain);

                // Get images
                if (auto res = vulkanDevice->getSwapchainImagesKHR(*swapchain, &imageCount, nullptr);
                    res != vk::Result::eSuccess)
                {
                }
                swapchainImages.resize(imageCount);
                if (auto res = vulkanDevice->getSwapchainImagesKHR(*swapchain, &imageCount, swapchainImages.data());
                    res != vk::Result::eSuccess)
                {
                }

                // Recreate NVRHI textures
                swapchainTexDesc.setWidth(width).setHeight(height);
                for (uint32_t i = 0; i < imageCount; ++i)
                {
                    VkImage              nativeImage = static_cast<VkImage>(swapchainImages[i]);
                    nvrhi::TextureHandle tex = nvrhiDevice->createHandleForNativeTexture(nvrhi::ObjectTypes::VK_Image,
                                                                                         nvrhi::Object(nativeImage),
                                                                                         swapchainTexDesc);
                    swapchainTextures.push_back(tex);
                }

                // Recreate depth
                depthDesc.setWidth(width).setHeight(height);
                depthTexture = nvrhiDevice->createTexture(depthDesc);

                // Recreate framebuffers
                for (uint32_t i = 0; i < imageCount; ++i)
                {
                    auto fbDesc = nvrhi::FramebufferDesc().addColorAttachment(swapchainTextures[i]);
                    fbDesc.setDepthAttachment(depthTexture);
                    framebuffers.push_back(nvrhiDevice->createFramebuffer(fbDesc));
                }
            }
        }

        // Update ViewProjection
        {
            ViewProjection vp{};
            std::fill(std::begin(vp.viewProj), std::end(vp.viewProj), 0.0f);

            // Simple orthographic projection to keep aspect ratio
            float aspect = static_cast<float>(swapchainExtent.width) / static_cast<float>(swapchainExtent.height);
            float scaleX = 1.0f;
            float scaleY = 1.0f;

            if (aspect > 1.0f)
                scaleX /= aspect;
            else
                scaleY *= aspect;

            vp.viewProj[0]  = scaleX;
            vp.viewProj[5]  = scaleY;
            vp.viewProj[10] = 1.0f;
            vp.viewProj[15] = 1.0f;

            cmdList->open();
            cmdList->writeBuffer(viewProjBuffer, &vp, sizeof(vp));
            cmdList->close();
            nvrhiDevice->executeCommandList(cmdList);
        }

        // wait for previous frame work to finish
        if (vulkanDevice->waitForFences(*frameFences[currentFrame], VK_TRUE, std::numeric_limits<uint64_t>::max()) !=
            vk::Result::eSuccess)
        {
            logger->critical("Failed to wait for fences");
            return -1;
        }

        // Acquire image
        uint32_t   imageIndex = 0;
        vk::Result res;

        res = vulkanDevice->acquireNextImageKHR(*swapchain,
                                                std::numeric_limits<uint64_t>::max(),
                                                *imageAcquiredSemaphores[currentFrame],
                                                vk::Fence(),
                                                &imageIndex);

        if (res == vk::Result::eErrorOutOfDateKHR || res == vk::Result::eSuboptimalKHR)
        {
            g_ResizeRequested = true;
            // If we acquired an image (Suboptimal), the semaphore is signaled.
            // If we resize, we discard the frame. We should recreate the semaphore to clear its state.
            imageAcquiredSemaphores[currentFrame] = vulkanDevice->createSemaphoreUnique({});
            continue;
        }
        else if (res != vk::Result::eSuccess)
        {
            logger->critical("Failed to acquire image");
            return -1;
        }

        vulkanDevice->resetFences(*frameFences[currentFrame]);

        nvrhiDevice->queueWaitForSemaphore(nvrhi::CommandQueue::Graphics, *imageAcquiredSemaphores[currentFrame], 0);

        // Render
        cmdList->open();
        auto fb = framebuffers[imageIndex];
        nvrhi::utils::ClearColorAttachment(cmdList, fb, 0, nvrhi::Color(0.f, 0.f, 0.f, 1.f));
        cmdList->clearDepthStencilTexture(depthTexture, nvrhi::AllSubresources, true, 1.0f, false, 0);


        auto graphicsState =
                nvrhi::GraphicsState()
                        .setPipeline(graphicsPipeline)
                        .setFramebuffer(fb)
                        .setViewport(nvrhi::ViewportState().addViewportAndScissorRect(
                                nvrhi::Viewport(static_cast<float>(swapchainExtent.width),
                                                static_cast<float>(swapchainExtent.height))))
                        .addBindingSet(bindingSet)
                        .addVertexBuffer(nvrhi::VertexBufferBinding().setBuffer(vertexBuffer).setSlot(0).setOffset(0));

        cmdList->setGraphicsState(graphicsState);
        cmdList->draw(nvrhi::DrawArguments().setVertexCount(6).setInstanceCount(1));
        cmdList->close();

        nvrhiDevice->queueSignalSemaphore(nvrhi::CommandQueue::Graphics, *renderCompletedSemaphores[currentFrame], 0);
        nvrhiDevice->executeCommandList(cmdList);

        // Submit fence
        // NVRHI doesn't expose fence submission directly in executeCommandList for external fences easily without using
        // queueSubmit But we can use vulkanDevice->getQueue...submit Wait, NVRHI executeCommandList submits to the
        // queue. We need to signal the fence. NVRHI doesn't seem to have a "signal fence" API easily accessible on
        // CommandList? We can just submit an empty batch to signal the fence? Or better: NVRHI's executeCommandList
        // doesn't take a fence. We can use the underlying queue to submit the fence. But we need to ensure ordering.
        // NVRHI submits to the queue.
        // If we submit to the same queue, it is serialized.

        vulkanDevice->getQueue(graphicsFamilyIndex, 0).submit(vk::SubmitInfo(), *frameFences[currentFrame]);


        vk::Semaphore      waitSem     = *renderCompletedSemaphores[currentFrame];
        vk::PresentInfoKHR presentInfo = vk::PresentInfoKHR()
                                                 .setWaitSemaphoreCount(1)
                                                 .setPWaitSemaphores(&waitSem)
                                                 .setSwapchainCount(1)
                                                 .setPSwapchains(&*swapchain)
                                                 .setPImageIndices(&imageIndex);

        auto presentResult = vulkanDevice->getQueue(graphicsFamilyIndex, 0).presentKHR(presentInfo);

        if (presentResult == vk::Result::eErrorOutOfDateKHR || presentResult == vk::Result::eSuboptimalKHR)
        {
            g_ResizeRequested = true;
        }
        else if (presentResult != vk::Result::eSuccess)
        {
            logger->critical("Failed to present");
        }

        // TODO: if vsync or debugruntime, explicitly sync queue with waitidle
        nvrhiDevice->runGarbageCollection();

        currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;

        // TODO: frames in flight
        // TODO: EventQueryHandle. reset and push

        uint64_t endCycles = __rdtsc();
        cyclesSinceLastReport += endCycles - startCycles;
        framesSinceLastReport++;

        auto                                      now     = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> elapsed = now - lastReportTime;

        if (elapsed.count() >= 1000.0)
        {
            double fps        = framesSinceLastReport / (elapsed.count() / 1000.0);
            double msPerFrame = elapsed.count() / framesSinceLastReport;
            double avgCycles  = static_cast<double>(cyclesSinceLastReport) / framesSinceLastReport;

            logger->info("FPS: {:.2f}, Avg ms: {:.2f}, Avg cycles: {:.0f}", fps, msPerFrame, avgCycles);

            lastReportTime        = now;
            framesSinceLastReport = 0;
            cyclesSinceLastReport = 0;
        }
    }


    nvrhiDevice->waitForIdle();
    vulkanDevice->waitIdle();

    framebuffers.clear();
    swapchainTextures.clear();

    graphicsPipeline = nullptr;
    cmdList          = nullptr;

    // TODO: Global nvrhi stuff needs to be cleaned up before main exits
    nvrhiDevice = nullptr;
    glfwDestroyWindow(windowHandle);
    glfwTerminate();

    return 0;
}

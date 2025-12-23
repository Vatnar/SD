#include <spdlog/spdlog.h>
#include <spdlog/async.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>


#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#define VULKAN_HPP_ENABLE_DYNAMIC_LOADER_TOOL 1
#include <vulkan/vulkan.hpp>
VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE
#include <nvrhi/vulkan.h>

#include <GLFW/glfw3.h>

#include <nvrhi/utils.h>
#include <nvrhi/validation.h>

#include <iostream>
#include <print>
#include <queue>


#include "assets/shaders/vertex_shader.h"
#include "assets/shaders/pixel_shader.h"


constexpr bool enableValidation = true;


extern unsigned char vertex_spv[], pixel_spv[];
extern unsigned int  vertex_spv_len, pixel_spv_len;

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

// TODO: Custom logging macros that respect log levels and remove calls
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

int main()
{
    // NOTE: Init logging

    init_logging();
    auto logger = spdlog::get("engine");

    // TODO: Create window and such


    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow *windowHandle = glfwCreateWindow(800, 600, "SDPrototype", nullptr, nullptr);


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

    // TODO: actually query for extensions, and not just assume they exist
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
    vk::SurfaceKHR surface = rawSurface;


    // NOTE: setup queues
    // todo: find graphics family index

    auto     queueFamilies       = physDev.getQueueFamilyProperties();
    uint32_t graphicsFamilyIndex = UINT32_MAX;

    for (uint32_t i = 0; i < queueFamilies.size(); ++i)
    {
        bool supportsGraphics =
                (queueFamilies[i].queueFlags & vk::QueueFlagBits::eGraphics) == vk::QueueFlagBits::eGraphics;

        vk::Bool32 supportsPresent = vk::False;

        if (auto res = physDev.getSurfaceSupportKHR(i, surface, &supportsPresent); res != vk::Result::eSuccess)
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
    nvrhiDesc.deviceExtensions    = deviceExts.data();
    nvrhiDesc.numDeviceExtensions = deviceExts.size();

    // NVRHI Wrapper
    nvrhi::DeviceHandle nvrhiDevice = nvrhi::vulkan::createDevice(nvrhiDesc);


    // NOTE: VALIDATION:
    if (enableValidation)
    {
        nvrhi::DeviceHandle nvrhiValidationLayer = nvrhi::validation::createValidationLayer(nvrhiDevice);
        nvrhiDevice                              = nvrhiValidationLayer;
    }

    // endregion


    // Find swapchain width and height

    vk::SurfaceCapabilitiesKHR caps = physDev.getSurfaceCapabilitiesKHR(surface);
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
    auto surfaceFormats = physDev.getSurfaceFormatsKHR(surface);

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
    auto               presentModes = physDev.getSurfacePresentModesKHR(surface);
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
    swapchainCreateInfo.setSurface(surface)
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

    vk::SwapchainKHR swapchain = vulkanDevice->createSwapchainKHR(swapchainCreateInfo);

    // Get all swapchain images
    uint32_t imageCount = 0;

    // TODO: Check if we might aswell just put it into the vector right away, if that is better or not
    // TODO: Get/create swap chain

    if (auto res = vulkanDevice->getSwapchainImagesKHR(swapchain, &imageCount, nullptr); res != vk::Result::eSuccess)
    {
        logger->warn("getSwapChainImagesKHR returned vk::Result::{}", static_cast<uint64_t>(res));
    }
    std::vector<vk::Image> swapchainImages(imageCount);

    if (auto res = vulkanDevice->getSwapchainImagesKHR(swapchain, &imageCount, swapchainImages.data());
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

    // NOTE: Graphics Pipeline

    // todo: AUtomate compilation somehow

    nvrhi::ShaderHandle vertexShader =
            nvrhiDevice->createShader(nvrhi::ShaderDesc().setShaderType(nvrhi::ShaderType::Vertex),
                                      vertex_spv,
                                      vertex_spv_len);

    nvrhi::ShaderHandle pixelShader =
            nvrhiDevice->createShader(nvrhi::ShaderDesc().setShaderType(nvrhi::ShaderType::Pixel),
                                      pixel_spv,
                                      pixel_spv_len);

    // nvrhi::VertexAttributeDesc attributes[] = {
    //         nvrhi::VertexAttributeDesc()
    //                 .setName("POSITION")
    //                 .setFormat(nvrhi::Format::RGB32_FLOAT)
    //                 .setOffset(offsetof(Vertex, position))
    //                 .setElementStride(sizeof(Vertex)),
    //         nvrhi::VertexAttributeDesc()
    //                 .setName("TEXCOORD")
    //                 .setFormat(nvrhi::Format::RG32_FLOAT)
    //                 .setOffset(offsetof(Vertex, texCoord))
    //                 .setElementStride(sizeof(Vertex)),
    // };

    nvrhi::InputLayoutHandle inputLayout = nvrhiDevice->createInputLayout(nullptr, 0, vertexShader);

    auto fbInfo = framebuffers[0]->getFramebufferInfo();
    logger->debug("colorFormats[0]={}", static_cast<uint32_t>(fbInfo.colorFormats[0]));

    // CORRECT pipeline creation:
    auto pipelineDesc = nvrhi::GraphicsPipelineDesc()
                                .setInputLayout(inputLayout)
                                .setVertexShader(vertexShader)
                                .setPixelShader(pixelShader);

    pipelineDesc.renderState.rasterState.setCullMode(nvrhi::RasterCullMode::None);

    // NO targetCount loop - your version INFERS from fbInfo in createGraphicsPipeline
    nvrhi::GraphicsPipelineHandle graphicsPipeline = nvrhiDevice->createGraphicsPipeline(pipelineDesc, fbInfo);


    // NOTE: Binding layout
    // auto layoutDesc = nvrhi::BindingLayoutDesc()
    // .setVisibility(nvrhi::ShaderType::All)
    // .addItem(nvrhi::BindingLayoutItem::Texture_SRV(0))             // texture at t0
    // .addItem(nvrhi::BindingLayoutItem::VolatileConstantBuffer(0)); // constants at b0

    // nvrhi::BindingLayoutHandle bindingLayout = nvrhiDevice->createBindingLayout(layoutDesc);

    //
    //
    // // NOTE: Creating the resources
    //
    // // CVB, constant volatile buffer
    // auto constantBufferDesc = nvrhi::BufferDesc()
    //                                   .setByteSize(sizeof(float) * 16) // stores one matrix
    //                                   .setIsConstantBuffer(true)
    //                                   .setIsVolatile(true)
    //                                   .setMaxVersions(16); // number of automatic versions, only necessary on VULKAN
    //
    // nvrhi::BufferHandle constantBuffer = nvrhiDevice->createBuffer(constantBufferDesc);
    //
    // // vertex buffer
    // static const Vertex g_Vertices[] = {
    //         //  position          texCoord
    //         {{0.f, 0.f, 0.f}, {0.f, 0.f}},
    //         {{1.f, 0.f, 0.f}, {1.f, 0.f}},
    //         // ...
    // };
    //
    // auto vertexBufferDesc = nvrhi::BufferDesc()
    //                                 .setByteSize(sizeof(g_Vertices))
    //                                 .setIsVertexBuffer(true)
    //                                 .enableAutomaticStateTracking(nvrhi::ResourceStates::VertexBuffer)
    //                                 .setDebugName("Vertex Buffer");
    //
    // nvrhi::BufferHandle vertexBuffer = nvrhiDevice->createBuffer(vertexBufferDesc);
    //
    // // texture
    // // assuming texture pixel data is loaded and decoded elsewhere.
    // auto textureDesc = nvrhi::TextureDesc()
    //                            .setDimension(nvrhi::TextureDimension::Texture2D)
    //                            .setWidth(textureWidth)
    //                            .setHeight(textureHeight)
    //                            .setFormat(nvrhi::Format::SRGBA8_UNORM)
    //                            .enableAutomaticStateTracking(nvrhi::ResourceStates::ShaderResource)
    //                            .setDebugName("Geometry Texture");
    //
    // nvrhi::TextureHandle geometryTexture = nvrhiDevice->createTexture(textureDesc);

    //
    // // NOTE: Command list
    // nvrhi::CommandListHandle commandList = nvrhiDevice->createCommandList();
    //
    //
    // // NOTE: Bind resources to graphics pipline at draw time
    // auto bindingSetDesc = nvrhi::BindingSetDesc()
    //                               .addItem(nvrhi::BindingSetItem::Texture_SRV(0, geometryTexture))
    //                               .addItem(nvrhi::BindingSetItem::ConstantBuffer(0, constantBuffer));
    //
    // nvrhi::BindingSetHandle bindingSet = nvrhiDevice->createBindingSet(bindingSetDesc, bindingLayout);
    //
    //
    // // NOTE: Filling resource data (at initialization)
    // commandList->open();
    //
    // commandList->writeBuffer(vertexBuffer, g_Vertices, sizeof(g_Vertices));
    //
    // const void  *textureData     = ...;
    // const size_t textureRowPitch = ...;
    //     commandList->writeTexture(geometryTexture,
    /*arraySlide =  0,
    /*miplevel =  0,
    textureData,
    textureRowPitch);
commandList->close();
nvrhiDevice->executeCommandList(commandList);


// NOTE: Presentation functions...
// TODO:
https://github.com/NVIDIA-RTX/Donut-Samples/blob/main/examples/vertex_buffer/vertex_buffer.cpp
*/


    // sync objects
    vk::SemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.flags = {};
    semaphoreInfo.pNext = nullptr; // important, but you already do this

    vk::UniqueSemaphore imageAvailable = vulkanDevice->createSemaphoreUnique(semaphoreInfo);
    vk::UniqueSemaphore renderFinished = vulkanDevice->createSemaphoreUnique(semaphoreInfo);

    vk::UniqueFence inFlight = vulkanDevice->createFenceUnique(vk::FenceCreateInfo(vk::FenceCreateFlagBits::eSignaled));
    vulkanDevice->resetFences(*inFlight);
    nvrhi::CommandListHandle cmdList = nvrhiDevice->createCommandList();

    bool     isRunning  = true;
    uint32_t frameCount = 0;

    while (isRunning)
    {
        // TODO: Begin pass somehow?
        isRunning = !glfwWindowShouldClose(windowHandle);
        glfwPollEvents();

        // Acquire image
        uint32_t imageIndex = 0;
        auto     res = vulkanDevice->acquireNextImageKHR(swapchain, UINT64_MAX, VK_NULL_HANDLE, *inFlight, &imageIndex);
        if (res == vk::Result::eErrorOutOfDateKHR)
            continue;
        if (res != vk::Result::eSuccess)
            continue;

        // Wait for ACQUIRE fence IMMEDIATELY
        if (auto res = vulkanDevice->waitForFences(*inFlight, VK_TRUE, UINT64_MAX); res != vk::Result::eSuccess)
        {
            logger->warn("waitForFences returned vk::Result::{}", static_cast<uint64_t>(res));
        }
        vulkanDevice->resetFences(*inFlight);

        // Render
        cmdList->open();
        auto fb = framebuffers[imageIndex];
        nvrhi::utils::ClearColorAttachment(cmdList, fb, 0, nvrhi::Color(0.f));
        auto graphicsState = nvrhi::GraphicsState()
                                     .setPipeline(graphicsPipeline)
                                     .setFramebuffer(fb)
                                     .setViewport(nvrhi::ViewportState().addViewportAndScissorRect(
                                             nvrhi::Viewport(static_cast<float>(swapchainExtent.width),
                                                             static_cast<float>(swapchainExtent.height))));

        cmdList->setGraphicsState(graphicsState);
        cmdList->draw(nvrhi::DrawArguments().setVertexCount(3));
        cmdList->close();

        nvrhiDevice->executeCommandList(cmdList);

        //  Present
        vk::PresentInfoKHR presentInfo{};
        presentInfo.setSwapchainCount(1).setSwapchains(swapchain).setImageIndices(imageIndex);
        auto presentResult = vulkanDevice->getQueue(graphicsFamilyIndex, 0).presentKHR(presentInfo);
        if (presentResult != vk::Result::eSuccess)
        {
            // TODO: make custom loggin stuff
            std::println("PresentKHR error: vk::result::{}", static_cast<uint64_t>(presentResult));
        }
    }


    nvrhiDevice->waitForIdle();
    vulkanDevice->waitIdle();

    framebuffers.clear();
    swapchainTextures.clear();

    graphicsPipeline = nullptr;
    cmdList          = nullptr;

    vulkanDevice->destroySwapchainKHR(swapchain);
    instance->destroySurfaceKHR(surface);

    // TODO: Global nvrhi stuff needs to be cleaned up before main exits
    nvrhiDevice = nullptr;
    glfwDestroyWindow(windowHandle);
    glfwTerminate();

    return 0;
}

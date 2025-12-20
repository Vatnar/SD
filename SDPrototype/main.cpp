#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#define VULKAN_HPP_ENABLE_DYNAMIC_LOADER_TOOL 1
#include <vulkan/vulkan.hpp>
#include <nvrhi/vulkan.h>

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE


#include "nvrhi/utils.h"
#include "nvrhi/validation.h"

#include <iostream>
#include <print>
#include <queue>


const bool enableValidation = true;


const char g_VertexShader[] = "";
const char g_PixelShader[]  = "";


struct Vertex
{
    float position[3];
    float texCoord[2];
};

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

int main()
{
    // NOTE: INIT VULKAN

    static vk::detail::DynamicLoader dl;
    PFN_vkGetInstanceProcAddr        vkGetInstanceProcAddr =
            dl.getProcAddress<PFN_vkGetInstanceProcAddr>("vkGetInstanceProcAddr");

    VULKAN_HPP_DEFAULT_DISPATCHER.init(vkGetInstanceProcAddr);

    // Create instance
    vk::ApplicationInfo appInfo("MyEngine", 1, "NoEngine", 1, VK_API_VERSION_1_3);

    std::vector<const char *> instanceExts = {VK_KHR_SURFACE_EXTENSION_NAME,
                                              VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
                                              VK_EXT_DEBUG_REPORT_EXTENSION_NAME};

    vk::InstanceCreateInfo instInfo({}, &appInfo, {}, instanceExts);
    vk::UniqueInstance     instance = vk::createInstanceUnique(instInfo);

    VULKAN_HPP_DEFAULT_DISPATCHER.init(*instance);

    // physical device
    vk::PhysicalDevice physDev = instance->enumeratePhysicalDevices().front();

    // Create device

    vk::PhysicalDeviceVulkan13Features features13;
    features13.setDynamicRendering(true).setSynchronization2(true);

    vk::PhysicalDeviceFeatures2 features2;
    features2.setPNext(&features13);

    std::vector<const char *> deviceExts = {VK_KHR_SWAPCHAIN_EXTENSION_NAME,
                                            VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
                                            VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
                                            VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME};

    // NOTE: setup queues
    // todo: find graphics family index
    uint32_t graphicsFamilyIndex = 0;


    float                     priority = 1.0f;
    vk::DeviceQueueCreateInfo queueInfo({}, graphicsFamilyIndex, 1, &priority);

    vk::DeviceCreateInfo devInfo({}, queueInfo, {}, deviceExts);
    devInfo.setPNext(&features2);


    vk::UniqueDevice device = physDev.createDeviceUnique(devInfo);

    // init dispatcher for device
    VULKAN_HPP_DEFAULT_DISPATCHER.init(*device);


    // NOTE: Creating NVRHI device
    nvrhi::vulkan::DeviceDesc nvrhiDesc;
    nvrhiDesc.errorCB        = &g_MessageCallback;
    nvrhiDesc.instance       = *instance;
    nvrhiDesc.physicalDevice = physDev;
    nvrhiDesc.device         = *device;

    // queue
    nvrhiDesc.graphicsQueue      = device->getQueue(graphicsFamilyIndex, 0);
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
    /*
        // NOTE: Create swap chain textures
        auto textureDesc = nvrhi::TextureDesc()
                                   .setDimension(nvrhi::TextureDimension::Texture2D)
                                   .setFormat(nvrhi::Format::RGBA8_UNORM)
                                   .setWidth(swapChainWidth)
                                   .setHeight(swapChainHeight)
                                   .setIsRenderTarget(true)
                                   .setDebugName("Swap Chain Image");

        nvrhi::TextureHandle swapChainTexture =
                nvrhiDevice->createHandleForNativeTexture(nvrhi::ObjectTypes::VK_Image,
                                                          nativeTextureOrImage,
                                                          textureDesc);

        auto framebufferDesc                 =
       nvrhi::FramebufferDesc().addColorAttachment(swapChainTexture); nvrhi::FramebufferHandle framebuffer
       = nvrhiDevice->createFramebuffer(framebufferDesc);


        // NOTE: Graphics Pipeline
        nvrhi::ShaderHandle vertexShader =
                nvrhiDevice->createShader(nvrhi::ShaderDesc().setShaderType(nvrhi::ShaderType::Vertex),
                                          g_VertexShader,
                                          sizeof(g_VertexShader));

        nvrhi::VertexAttributeDesc attributes[] = {
                nvrhi::VertexAttributeDesc()
                        .setName("POSITION")
                        .setFormat(nvrhi::Format::RGB32_FLOAT)
                        .setOffset(offsetof(Vertex, position))
                        .setElementStride(sizeof(Vertex)),
                nvrhi::VertexAttributeDesc()
                        .setName("TEXCOORD")
                        .setFormat(nvrhi::Format::RG32_FLOAT)
                        .setOffset(offsetof(Vertex, texCoord))
                        .setElementStride(sizeof(Vertex)),
        };

        nvrhi::InputLayoutHandle inputLayout =
                nvrhiDevice->createInputLayout(attributes, uint32_t(std::size(attributes)), vertexShader);

        nvrhi::ShaderHandle pixelShader =
                nvrhiDevice->createShader(nvrhi::ShaderDesc().setShaderType(nvrhi::ShaderType::Pixel),
                                          g_PixelShader,
                                          sizeof(g_PixelShader));


        auto framebufferInfo = framebuffer->getFramebufferInfo();

        // NOTE: Binding layout
        auto layoutDesc =
                nvrhi::BindingLayoutDesc()
                        .setVisibility(nvrhi::ShaderType::All)
                        .addItem(nvrhi::BindingLayoutItem::Texture_SRV(0))             // texture at t0
                        .addItem(nvrhi::BindingLayoutItem::VolatileConstantBuffer(0)); // constants at b0

        nvrhi::BindingLayoutHandle bindingLayout = nvrhiDevice->createBindingLayout(layoutDesc);

        // NOTE: Pipeline creation
        auto pipelineDesc = nvrhi::GraphicsPipelineDesc()
                                    .setInputLayout(inputLayout)
                                    .setVertexShader(vertexShader)
                                    .setPixelShader(pixelShader)
                                    .addBindingLayout(bindingLayout);

        nvrhi::GraphicsPipelineHandle graphicsPipline =
                nvrhiDevice->createGraphicsPipeline(pipelineDesc, framebufferInfo);

        // NOTE: Creating the resources

        // CVB, constant volatile buffer
        auto constantBufferDesc =
                nvrhi::BufferDesc()
                        .setByteSize(sizeof(float) * 16) // stores one matrix
                        .setIsConstantBuffer(true)
                        .setIsVolatile(true)
                        .setMaxVersions(16); // number of automatic versions, only necessary on VULKAN

        nvrhi::BufferHandle constantBuffer = nvrhiDevice->createBuffer(constantBufferDesc);

        // vertex buffer
        static const Vertex g_Vertices[] = {
                //  position          texCoord
                {{0.f, 0.f, 0.f}, {0.f, 0.f}},
                {{1.f, 0.f, 0.f}, {1.f, 0.f}},
                // ...
        };

        auto vertexBufferDesc = nvrhi::BufferDesc()
                                        .setByteSize(sizeof(g_Vertices))
                                        .setIsVertexBuffer(true)
                                        .enableAutomaticStateTracking(nvrhi::ResourceStates::VertexBuffer)
                                        .setDebugName("Vertex Buffer");

        nvrhi::BufferHandle vertexBuffer = nvrhiDevice->createBuffer(vertexBufferDesc);

        // texture
        // assuming texture pixel data is loaded and decoded elsewhere.
        auto textureDesc = nvrhi::TextureDesc()
                                   .setDimension(nvrhi::TextureDimension::Texture2D)
                                   .setWidth(textureWidth)
                                   .setHeight(textureHeight)
                                   .setFormat(nvrhi::Format::SRGBA8_UNORM)
                                   .enableAutomaticStateTracking(nvrhi::ResourceStates::ShaderResource)
                                   .setDebugName("Geometry Texture");

        nvrhi::TextureHandle geometryTexture = nvrhiDevice->createTexture(textureDesc);


        // NOTE: Command list
        nvrhi::CommandListHandle commandList = nvrhiDevice->createCommandList();


        // NOTE: Bind resources to graphics pipline at draw time
        auto bindingSetDesc = nvrhi::BindingSetDesc()
                                      .addItem(nvrhi::BindingSetItem::Texture_SRV(0, geometryTexture))
                                      .addItem(nvrhi::BindingSetItem::ConstantBuffer(0, constantBuffer));

        nvrhi::BindingSetHandle bindingSet = nvrhiDevice->createBindingSet(bindingSetDesc, bindingLayout);


        // NOTE: Filling resource data (at initialization)
        commandList->open();

        commandList->writeBuffer(vertexBuffer, g_Vertices, sizeof(g_Vertices));

        const void  *textureData     = ...;
        const size_t textureRowPitch = ...;
        commandList->writeTexture(geometryTexture,
                                  /*arraySlide =  0,
                                  /*miplevel =  0,
                                  textureData,
                                  textureRowPitch);
        commandList->close();
        nvrhiDevice->executeCommandList(commandList);


        // NOTE: Rendering (drawing) geometry
        nvrhi::IFramebuffer *currentFramebuffer = ...;
        commandList->open();

        nvrhi::utils::ClearColorAttachment(commandList, currentFramebuffer, 0, nvrhi::Color(0.f));

        float viewProjectionMatrix[16] = {...};
        commandList->writeBuffer(constantBuffer, viewProjectionMatrix, sizeof(viewProjectionMatrix));


        // set graphics state, pipline, framebuffer, viewport, bindings
        auto graphicsState = nvrhi::GraphicsState()
                                     .setPipeline(graphicsPipline)
                                     .setFramebuffer(currentFramebuffer)
                                     .setViewport(nvrhi::ViewportState().addViewportAndScissorRect(
                                             nvrhi::Viewport(windowWidth, windowHeight)))
                                     .addBindingSet(bindingSet)
                                     .addVertexBuffer(vertexBuffer);

        commandList->setGraphicsState(graphicsState);

        // draw
        auto drawArguments = nvrhi::DrawArguments().setVertexCount(std::size(g_Vertices));
        commandList->draw(drawArguments);

        commandList->close();
        nvrhiDevice->executeCommandList(commandList);

        // NOTE: Presentation functions...
        // TODO:
       https://github.com/NVIDIA-RTX/Donut-Samples/blob/main/examples/vertex_buffer/vertex_buffer.cpp
    */

    device->waitIdle();
    // TODO: Global nvrhi stuff needs to be cleaned up before main exits
    nvrhiDevice = nullptr;

    return 0;
}

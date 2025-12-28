#include "VulkanConfig.hpp"
#include <vulkan/vulkan.hpp>

// 2) Define storage exactly once in the whole program
VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE;
// NVRHI
#include <nvrhi/vulkan.h>


#include <nvrhi/utils.h>
#include <nvrhi/validation.h>

// GLFW
#define GLFW_INCLUDE_VULKAN
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
#include "GlfwContext.hpp"
#include "VulkanContext.hpp"
#include "MessageCallback.hpp"
#include "Logging.hpp"
#include "ShaderCompiler.hpp"
#include "Window.hpp"


constexpr bool enableValidation     = true;
constexpr int  MAX_FRAMES_IN_FLIGHT = 2;

struct Vertex
{
    float position[3];
    float texCoord[2];
};


nvrhi::Format toNvrhiFormat(const vk::Format fmt)
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


MessageCallback  g_MessageCallback;
VkPhysicalDevice vulkanPhysicalDevice = nullptr;


bool g_ResizeRequested = false;

void framebufferResizeCallback(int width, int height)
{
    g_ResizeRequested = true;
}


nvrhi::TextureHandle CreateTexture(const nvrhi::vulkan::DeviceHandle& nvrhiDevice, std::filesystem::path filePath)
{
    auto logger = spdlog::get("engine");

    auto cmdList = nvrhiDevice->createCommandList();
    int  texWidth, texHeight, texChannels;

    if (stbi_uc *texPixels = stbi_load(filePath.c_str(), &texWidth, &texHeight, &texChannels, 4))
    {
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

    logger->error("Failed to load texture: {} (CWD: {})", filePath.string(), std::filesystem::current_path().string());

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

int main()
{
    // NOTE: Init logging

    init_logging();
    auto logger = spdlog::get("engine");

    // TODO: Create window and such

    GlfwContext glfwCtx;
    Window      window(800, 600, "SDPrototype");
    window.SetResizeCallback(framebufferResizeCallback);


    VulkanContext vulkanCtx(glfwCtx, window);


    // NOTE: Creating NVRHI device
    auto& vulkanDevice        = vulkanCtx.GetVulkanDevice();
    auto  graphicsFamilyIndex = vulkanCtx.GetGraphicsFamilyIndex();
    auto  deviceExts          = vulkanCtx.GetDeviceExtensions();

    nvrhi::vulkan::DeviceDesc nvrhiDesc;
    nvrhiDesc.errorCB        = &g_MessageCallback;
    nvrhiDesc.instance       = *vulkanCtx.GetInstance();
    nvrhiDesc.physicalDevice = vulkanCtx.GetPhysicalDevice();
    nvrhiDesc.device         = vulkanDevice.get();

    // queue
    nvrhiDesc.graphicsQueue      = vulkanDevice->getQueue(graphicsFamilyIndex, 0);
    nvrhiDesc.graphicsQueueIndex = static_cast<int>(graphicsFamilyIndex);

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
        // nvrhiDevice = nvrhiValidationLayer;
    }


    auto& swapchain = vulkanCtx.CreateSwapchain();
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

    auto     swapchainExtent = vulkanCtx.GetSwapchainExtent();
    uint32_t swapchainWidth  = swapchainExtent.width;
    uint32_t swapchainHeight = swapchainExtent.height;

    // TODO: this must match surface specs
    auto surfaceFormat           = vulkanCtx.GetSurfaceFormat();
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

    auto [windowWidth, windowHeight] = window.GetWindowSize();
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
    nvrhi::TextureHandle exampleTexture = CreateTexture(nvrhiDevice, "assets/textures/example.jpg");

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
        logger->critical("Failed to initialise shader compiler");
        nvrhiDevice->waitForIdle();
        return -1;
    }

    std::vector<char> vertexSpv;
    if (!compileShader("assets/shaders/vertex.hlsl", vertexSpv, "vs_6_0"))
    {
        logger->critical("Failed to compile vertex shader");
        nvrhiDevice->waitForIdle();
        return -1;
    }

    std::vector<char> pixelSpv;
    if (!compileShader("assets/shaders/pixel.hlsl", pixelSpv, "ps_6_0"))
    {
        logger->critical("Failed to compile pixel shader");
        nvrhiDevice->waitForIdle();
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
        isRunning = !window.ShouldClose();
        // TODO: Figure out how we do events, event stack?
        glfwPollEvents();

        if (g_ResizeRequested)
        {
            g_ResizeRequested    = false;
            auto [width, height] = window.GetWindowSize();
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

                auto& swapchainCreateInfo = vulkanCtx.GetSwapchainCreateInfo();
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
        nvrhi::utils::ClearColorAttachment(cmdList, fb, 0, nvrhi::Color(2.f, 0.f, 0.f, 1.f));
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

    // Clean up DXC
    g_dxcUtils.Release();
    g_dxcCompiler.Release();

    return 0;
}

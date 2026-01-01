
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

// STD
#include <iostream>
#include <queue>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <thread>
#include <memory>

// SD
#include "GlfwContext.hpp"
#include "VulkanContext.hpp"
#include "MessageCallback.hpp"
#include "Logging.hpp"
#include "ShaderCompiler.hpp"
#include "Window.hpp"

#include "Event.hpp"
#include "EventManager.hpp"
#include "LayerList.hpp"
#include "TestLayer.hpp"
#include "PerformanceLayer.hpp"

// STB

constexpr bool   enableValidation     = true;
constexpr int    MAX_FRAMES_IN_FLIGHT = 2;
constexpr double TARGET_FPS           = 60.0;


MessageCallback  g_MessageCallback;
VkPhysicalDevice vulkanPhysicalDevice = nullptr;


bool g_ResizeRequested = false;
int  main()
{
    // NOTE: Init logging

    init_logging();
    auto logger = spdlog::get("engine");

    // TODO: Create window and such


    GlfwContext glfwCtx;
    Window      window(800, 600, "SDPrototype");


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


    // region nvrhistuff
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


    // shared depth texture for all framebuffers
    // TODO: can we fix this in a nother way.

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

    // Create Layer Stack

    // TODO: I think we need some other safer way to access the layers.

    LayerList layers;
    layers.CreateAndAttachTop<TestLayer>(nvrhiDevice);
    auto testLayer = layers.GetRef<TestLayer>();
    if (testLayer == nullptr)
        throw std::runtime_error("Invalid testlayer");

    layers.CreateAndAttachTop<PerformanceLayer>();
    auto performanceLayer = layers.GetRef<PerformanceLayer>();
    if (performanceLayer == nullptr)
        throw std::runtime_error("Invalid performance layer");

    // Initial pipeline creation
    testLayer->UpdatePipeline(framebuffers[0]->getFramebufferInfo());


    // NOTE: EVENTS & callbacks
    std::vector<std::unique_ptr<Event>> eventManager;

    window.SetKeyCallback(
            [&eventManager](int key, int scancode, int action, int mods)
            {
                switch (action)
                {
                    case GLFW_PRESS:
                        eventManager.push_back(std::make_unique<KeyPressedEvent>(key, scancode, false));
                        break;
                    case GLFW_REPEAT:
                        eventManager.push_back(std::make_unique<KeyPressedEvent>(key, scancode, true));
                        break;
                    case GLFW_RELEASE:
                        eventManager.push_back(std::make_unique<KeyReleasedEvent>(key, scancode));
                        break;
                    default:
                        break;
                }
            });
    window.SetResizeCallback([](int, int) { g_ResizeRequested = true; });
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

    // endregion


    bool                      isRunning    = true;
    [[maybe_unused]] uint32_t frameCount   = 0;
    uint32_t                  currentFrame = 0;

    auto lastTime = std::chrono::high_resolution_clock::now();

    while (isRunning)
    {
        auto  now = std::chrono::high_resolution_clock::now();
        float dt  = std::chrono::duration<float>(now - lastTime).count();
        lastTime  = now;

        isRunning = !window.ShouldClose();

        // NOTE: Handle events, input etc
        // TODO: How are these handled etc.
        glfwPollEvents();


        // TODO: Poll events

        for (const auto& e : eventManager)
        {
            layers.HandleEvent(*e);
        }
        eventManager.clear();

        // NOTE: Rendering stuff
        // NOTE: Swapchain resize TODO: Where should i have this...
        if (g_ResizeRequested) [[unlikely]]
        {
            g_ResizeRequested = false;
            if (auto [width, height] = window.GetWindowSize(); width > 0 && height > 0)
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
                // TODO: Why do we need both? Is it not fine to just resize while getting them.
                if (auto res = vulkanDevice->getSwapchainImagesKHR(*swapchain, &imageCount, nullptr);
                    res != vk::Result::eSuccess)
                {
                    logger->error("Failed to get Swapchain images from vulkandevice : {}", static_cast<uint64_t>(res));
                }
                swapchainImages.resize(imageCount);
                if (auto res = vulkanDevice->getSwapchainImagesKHR(*swapchain, &imageCount, swapchainImages.data());
                    res != vk::Result::eSuccess)
                {
                    logger->error("Failed to get Swapchain images from vulkandevice : {}", static_cast<uint64_t>(res));
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

                // Recreate pipeline in layer
                testLayer->UpdatePipeline(framebuffers[0]->getFramebufferInfo());
            }
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
        nvrhiDevice->queueSignalSemaphore(nvrhi::CommandQueue::Graphics, *renderCompletedSemaphores[currentFrame], 0);

        // Render
        auto fb = framebuffers[imageIndex];

        testLayer->SetFramebuffer(fb);
        layers.Update(dt);
        layers.Render();

        // Submit fence

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


        // TODO: EventQueryHandle. reset and push

        // NOTE: Cap framerate

        auto                          frameEnd = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> elapsed  = frameEnd - now;
        std::chrono::duration<double> targetFrameTime(1.0 / TARGET_FPS);
        if (elapsed < targetFrameTime)
        {
            performanceLayer->BeginSleep();
            std::this_thread::sleep_for(targetFrameTime - elapsed);
            performanceLayer->EndSleep();
        }

        currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
    }


    nvrhiDevice->waitForIdle();
    vulkanDevice->waitIdle();

    framebuffers.clear();
    swapchainTextures.clear();

    // TODO: Global nvrhi stuff needs to be cleaned up before main exits
    nvrhiDevice = nullptr;

    // Clean up DXC
    g_dxcUtils.Release();
    g_dxcCompiler.Release();

    return 0;
}

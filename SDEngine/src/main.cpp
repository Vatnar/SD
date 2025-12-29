#include "LayerList.hpp"
#include "TestLayer.hpp"
#include "PerformanceLayer.hpp"
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


MessageCallback  g_MessageCallback;
VkPhysicalDevice vulkanPhysicalDevice = nullptr;


bool g_ResizeRequested = false;

void framebufferResizeCallback(int width, int height)
{
    g_ResizeRequested = true;
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

    // Create Layer Stack
    LayerList layers;
    auto      testLayer    = std::make_unique<TestLayer>(nvrhiDevice);
    auto     *testLayerPtr = testLayer.get();
    layers.AttachTop(std::move(testLayer));
    layers.AttachTop(std::make_unique<PerformanceLayer>());

    // Initial pipeline creation
    testLayerPtr->UpdatePipeline(framebuffers[0]->getFramebufferInfo());

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

        // TODO: Begin pass somehow?
        isRunning = !window.ShouldClose();
        // TODO: Figure out how we do events, event stack?
        glfwPollEvents();

        if (g_ResizeRequested) [[unlikely]]
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

                // Recreate pipeline in layer
                testLayerPtr->UpdatePipeline(framebuffers[0]->getFramebufferInfo());
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

        testLayerPtr->SetFramebuffer(fb);
        layers.Update(dt);
        layers.Render();

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

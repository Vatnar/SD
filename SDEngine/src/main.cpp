#include "EngineEvent.hpp"
#include "VulkanConfig.hpp"
#include <vulkan/vulkan.hpp>

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE;

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
#include <deque>

// SD
#include "GlfwContext.hpp"
#include "VulkanContext.hpp"
#include "Logging.hpp"
#include "ShaderCompiler.hpp"
#include "Window.hpp"

#include "InputEvent.hpp"
#include "EventManager.hpp"
#include "LayerList.hpp"
#include "TestLayer.hpp"
#include "PerformanceLayer.hpp"
#include "ImGuiLayer.hpp"
#include "DebugInfoLayer.hpp"

constexpr bool enableValidation = true;

// BUG: Works on > 2, but some validation errors
constexpr int    MAX_FRAMES_IN_FLIGHT = 2;
constexpr double TARGET_FPS           = 144.0;


VkPhysicalDevice vulkanPhysicalDevice = nullptr;


int main()
{
    // NOTE: Init logging

    init_logging();
    auto logger = spdlog::get("engine");
    if (!logger)
    {
        std::cout << "Failed to get logger \"engine\"" << '\n';
    }

    GlfwContext glfwCtx;
    Window      window{
                 {"SDEngine - Vulkan", 800, 600}
    };


    VulkanContext vulkanCtx{glfwCtx, window, MAX_FRAMES_IN_FLIGHT};

    // NOTE: Create and attach layers
    LayerList layers{};
    layers.CreateAndAttachTop<TestLayer>(vulkanCtx);
    auto testLayer = layers.GetRef<TestLayer>();

    layers.CreateAndAttachTop<PerformanceLayer>();
    auto performanceLayer = layers.GetRef<PerformanceLayer>();

    layers.CreateAndAttachTop<ImGuiLayer>(vulkanCtx, window);
    auto imguiLayer = layers.GetRef<ImGuiLayer>();

    layers.CreateAndAttachTop<DebugInfoLayer>();


    // NOTE: EVENTS & callbacks

    auto&              inputEventManager = window.GetEventManager();
    EngineEventManager engineEventManager;

    // NOTE: Create sync objects

    uint64_t frameCounter = 0;

    auto& vulkanDevice = vulkanCtx.GetVulkanDevice();


    // NOTE: MAIN LOOP

    window.SetResizeCallback([&](int w, int h) { engineEventManager.PushEvent<WindowResizeEvent>(w, h); });


    bool                      isRunning  = true;
    [[maybe_unused]] uint32_t frameCount = 0;

    uint32_t currentFrame = 0;

    auto lastTime = std::chrono::high_resolution_clock::now();

    while (isRunning)
    {
        auto  now = std::chrono::high_resolution_clock::now();
        float dt  = std::chrono::duration<float>(now - lastTime).count();
        lastTime  = now;

        isRunning = !window.ShouldClose();

        // NOTE: Handle events, input etc
        glfwPollEvents();

        // NOTE: Handle engine events

        if (engineEventManager.HasResizeEvent())
        {
            vulkanCtx.RecreateSwapchain(layers);
            vulkanCtx.RebuildPerImageSync();
            engineEventManager.ClearType<WindowResizeEvent>();
            engineEventManager.ClearType<SwapchainOutOfDateEvent>();
        }

        engineEventManager.Clear();

        // NOTE: Handle input events
        std::ranges::for_each(inputEventManager, [&](const auto& event) { layers.HandleEvent(*event); });
        inputEventManager.clear();

        auto& frameSync = vulkanCtx.GetFrameSync(currentFrame);

        // NOTE: Wait for last frame
        if (vulkanDevice->waitForFences(*frameSync.inFlight, VK_TRUE, std::numeric_limits<uint64_t>::max()) !=
            vk::Result::eSuccess)
        {
            logger->critical("Failed to wait for fences");
            Engine::Abort("Failed to wait for fences");
        } // logger->trace("Waited for fence frame {}", currentFrame);


        // NOTE: Get vulkan images

        uint32_t imageIndex = vulkanCtx.GetVulkanImages(engineEventManager, frameSync.imageAcquired);
        if (imageIndex == std::numeric_limits<uint32_t>::max())
        {
            logger->critical("Couldnt get image");
            continue;
        }

        auto& swapchainSync = vulkanCtx.GetSwapchainSync(imageIndex);

        vulkanDevice->resetFences(*frameSync.inFlight);


        // NOTE: Render
        imguiLayer->Begin();

        layers.Update(dt);
        layers.Render();

        // TODO: Abstract
        testLayer->RecordCommands(imageIndex, currentFrame);
        imguiLayer->RecordCommands(imageIndex, currentFrame);
        imguiLayer->End();

        // NOTE: SUBMIT


        // TODO: Abstract
        vk::CommandBuffer cmdBuffers[] = {
                testLayer->getCommandBuffer(currentFrame),
                imguiLayer->getCommandBuffer(currentFrame),
        };

        vk::Semaphore          waitSemaphores[] = {*frameSync.imageAcquired};
        vk::PipelineStageFlags waitStages[]     = {vk::PipelineStageFlagBits::eColorAttachmentOutput};

        vk::Semaphore signalSemaphores[] = {*swapchainSync.renderComplete};

        vk::SubmitInfo submitInfo{};
        submitInfo.setWaitSemaphores(waitSemaphores);
        submitInfo.setWaitDstStageMask(waitStages);
        submitInfo.setCommandBuffers(cmdBuffers);
        submitInfo.setSignalSemaphores(signalSemaphores);

        vulkanCtx.GetGraphicsQueue().submit(submitInfo, *frameSync.inFlight);

        // NOTE: Present
        vulkanCtx.PresentImage(engineEventManager, imageIndex);


        // NOTE: Performance stuff
        auto                          frameEnd = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> elapsed  = frameEnd - now;
        std::chrono::duration<double> targetFrameTime(1.0 / TARGET_FPS);
        if (elapsed < targetFrameTime)
        {
            performanceLayer->BeginSleep();
            std::this_thread::sleep_for(targetFrameTime - elapsed);
            performanceLayer->EndSleep();
        }

        // NOTE: Frame increment
        currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
        frameCounter++;
    }


    vulkanDevice->waitIdle();


    g_dxcUtils.Release();
    g_dxcCompiler.Release();

    return 0;
}

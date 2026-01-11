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

#include "Event.hpp"
#include "EventManager.hpp"
#include "LayerList.hpp"
#include "TestLayer.hpp"
#include "PerformanceLayer.hpp"
#include "ImGuiLayer.hpp"
#include "DebugInfoLayer.hpp"

constexpr bool   enableValidation     = true;
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

    // Create Layer Stack
    LayerList layers{};
    layers.CreateAndAttachTop<TestLayer>(vulkanCtx);
    auto testLayer = layers.GetRef<TestLayer>();

    layers.CreateAndAttachTop<PerformanceLayer>();
    auto performanceLayer = layers.GetRef<PerformanceLayer>();

    layers.CreateAndAttachTop<ImGuiLayer>(vulkanCtx, window);
    auto imguiLayer = layers.GetRef<ImGuiLayer>();

    layers.CreateAndAttachTop<DebugInfoLayer>();




    // NOTE: EVENTS & callbacks
    auto& eventManager = window.GetEventManager();

    // sync objects
    std::vector<vk::UniqueSemaphore> imageAcquiredSemaphores;
    std::vector<vk::UniqueSemaphore> testLayerCompleteSemaphores;
    std::vector<vk::UniqueSemaphore> renderCompletedSemaphores;
    std::vector<vk::UniqueFence>     frameFences;

    uint64_t frameCounter = 0;

    auto& vulkanDevice = vulkanCtx.GetVulkanDevice();

    // Initialize per-frame resources
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
    {
        imageAcquiredSemaphores.push_back(vulkanDevice->createSemaphoreUnique({}));
        testLayerCompleteSemaphores.push_back(vulkanDevice->createSemaphoreUnique({}));
        frameFences.push_back(vulkanDevice->createFenceUnique({vk::FenceCreateFlagBits::eSignaled}));
    }

    // Initialize per-swapchain-image resources
    for (size_t i = 0; i < vulkanCtx.GetSwapchainImages().size(); i++)
    {
        renderCompletedSemaphores.push_back(vulkanDevice->createSemaphoreUnique({}));
    }

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
        glfwPollEvents();


        // TODO: Poll events

        for (const auto& e : eventManager)
        {
            layers.HandleEvent(*e);
        }
        eventManager.clear();

        // NOTE: Rendering stuff
        // NOTE: Swapchain resize TODO: Where should i have this...
        if (window.ShouldResize()) [[unlikely]]
        {
            window.resizeRequested = false;
            if (auto [width, height] = window.GetWindowSize(); width > 0 && height > 0)
            {
                vulkanCtx.RecreateSwapchain();
                testLayer->OnSwapchainRecreated();
                imguiLayer->OnSwapchainRecreated();

                // Recreate render completion semaphores to match new swapchain image count
                renderCompletedSemaphores.clear();
                for (size_t i = 0; i < vulkanCtx.GetSwapchainImages().size(); i++)
                {
                    renderCompletedSemaphores.push_back(vulkanDevice->createSemaphoreUnique({}));
                }
            }
        }

        // wait for previous frame work to finish
        if (vulkanDevice->waitForFences(*frameFences[currentFrame], VK_TRUE, std::numeric_limits<uint64_t>::max()) !=
            vk::Result::eSuccess)
        {
            logger->critical("Failed to wait for fences");
            Engine::Abort("Failed to wait for fences");
        } // logger->trace("Waited for fence frame {}", currentFrame);


        uint32_t   imageIndex = 0;
        vk::Result res;

        try
        {
            auto resultValue = vulkanDevice->acquireNextImageKHR(*vulkanCtx.GetSwapchain(),
                                                                 std::numeric_limits<uint64_t>::max(),
                                                                 *imageAcquiredSemaphores[currentFrame],
                                                                 vk::Fence());
            res              = resultValue.result;
            imageIndex       = resultValue.value;
        }
        catch (vk::SystemError& err)
        {
            res = static_cast<vk::Result>(err.code().value());
        }

        if (res == vk::Result::eErrorOutOfDateKHR || res == vk::Result::eSuboptimalKHR)
        {
            window.resizeRequested = true;
            imageAcquiredSemaphores[currentFrame] =
                    vulkanDevice->createSemaphoreUnique({});
            continue;
        }
        else if (res != vk::Result::eSuccess && res != vk::Result::eSuboptimalKHR)
        {
            logger->critical("Failed to acquire image: {}", vk::to_string(res));
            continue;
        }

        vulkanDevice->resetFences(*frameFences[currentFrame]);

        imguiLayer->Begin();

        layers.Update(dt);
        layers.Render();

        testLayer->Render(imageIndex, *imageAcquiredSemaphores[currentFrame], *testLayerCompleteSemaphores[currentFrame], vk::Fence(), currentFrame);

        imguiLayer->End(imageIndex,
                        *testLayerCompleteSemaphores[currentFrame],
                        *renderCompletedSemaphores[imageIndex],
                        *frameFences[currentFrame],
                        currentFrame);

            vk::PresentInfoKHR presentInfo = vk::PresentInfoKHR()
                                                 .setWaitSemaphores(*renderCompletedSemaphores[imageIndex])
                                                 .setSwapchainCount(1)
                                                 .setPSwapchains(&*vulkanCtx.GetSwapchain())
                                                 .setPImageIndices(&imageIndex);

        vk::Result presentResult;
        try
        {
            presentResult = vulkanCtx.GetGraphicsQueue().presentKHR(presentInfo);
        }
        catch (vk::SystemError& err)
        {
            presentResult = static_cast<vk::Result>(err.code().value());
        }

        if (presentResult == vk::Result::eErrorOutOfDateKHR || presentResult == vk::Result::eSuboptimalKHR)
        {
            window.resizeRequested = true;
        }
        else if (presentResult != vk::Result::eSuccess)
        {
            logger->critical("Failed to present");
        }

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
        frameCounter++;
    }


    vulkanDevice->waitIdle();


    g_dxcUtils.Release();
    g_dxcCompiler.Release();

    return 0;
}

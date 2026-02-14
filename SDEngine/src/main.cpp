#include "Core/VulkanConfig.hpp"
#include "Layers/Shader2DLayer.hpp"
VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE;

#include "Core/Events/EngineEvent.hpp"


// GLFW
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

// STD
#include <chrono>
#include <filesystem>
#include <iostream>
#include <memory>
#include <thread>

// SD
#include "../include/Core/Events/EventManager.hpp"
#include "../include/Core/Vulkan/VulkanContext.hpp"
#include "Core/GlfwContext.hpp"
#include "Core/LayerList.hpp"
#include "Core/Logging.hpp"
#include "Core/ShaderCompiler.hpp"
#include "Core/Window.hpp"
#include "Layers/DebugInfoLayer.hpp"
#include "Layers/ImGuiLayer.hpp"
#include "Layers/PerformanceLayer.hpp"
#include "Layers/TestLayer.hpp"

constexpr bool enableValidation = true;

// BUG: Works on > 2, but some validation errors
constexpr int MAX_FRAMES_IN_FLIGHT = 2;
constexpr double TARGET_FPS = 144.0;


VkPhysicalDevice vulkanPhysicalDevice = nullptr;


int main() {
  // NOTE: Login INIT
  init_logging();
  auto logger = spdlog::get("engine");
  if (!logger) {
    std::cout << "Failed to get logger \"engine\"" << '\n';
  }

  // NOTE: GLFW, WINDOW, and VULKAN INIT
  GlfwContext glfwCtx;
  Window window{
      {"SDEngine - Vulkan", 800, 600}
  };


  VulkanContext vulkanCtx{glfwCtx, window, MAX_FRAMES_IN_FLIGHT};
  auto& vulkanDevice = vulkanCtx.GetVulkanDevice();


  // NOTE: Layerlist INIT
  LayerList layers{};
  // layers.CreateAndAttachTop<TestLayer>(vulkanCtx);
  std::vector<std::string> textures = {"assets/textures/CreateNoteInteraction.png",
                                       "assets/textures/example.jpg"};
  layers.CreateAndAttachTop<Shader2DLayer>(vulkanCtx, textures);

  layers.CreateAndAttachTop<PerformanceLayer>();
  auto performanceLayer = layers.GetRef<PerformanceLayer>();

  layers.CreateAndAttachTop<DebugInfoLayer>();

  layers.CreateAndAttachTop<ImGuiLayer>(vulkanCtx, window);
  auto imguiLayer = layers.GetRef<ImGuiLayer>();


  // NOTE: EVENTS & callbacks INIT

  auto& inputEventManager = window.GetEventManager();
  EngineEventManager engineEventManager;

  window.SetResizeCallback(
      [&](int w, int h) { engineEventManager.PushEvent<WindowResizeEvent>(w, h); });

  // NOTE: Rendering logic
  uint64_t currentFrame = 0;
  uint64_t frameCounter = 0;
  auto renderFrame = [&]() {
    // NOTE: Engine Events
    // resize if needed
    if (engineEventManager.HasResizeEvent()) {
      auto [width, height]{window.GetFramebufferSize()};
      if (width == 0 || height == 0)
        return;

      vulkanCtx.RecreateSwapchain(layers);
      vulkanCtx.RebuildPerImageSync();
      engineEventManager.ClearType<WindowResizeEvent>();
      engineEventManager.ClearType<SwapchainOutOfDateEvent>();
    }

    auto& [imageAcquired, inFlight] = vulkanCtx.GetFrameSync(currentFrame);

    // NOTE: Wait for last frame
    if (vulkanDevice->waitForFences(*inFlight, VK_TRUE, std::numeric_limits<uint64_t>::max()) !=
        vk::Result::eSuccess) {
      logger->critical("Failed to wait for fences");
      Engine::Abort("Failed to wait for fences");
    }

    // NOTE: Get vulkan images
    auto imageIndexRes = vulkanCtx.GetVulkanImages(engineEventManager, imageAcquired);
    switch (imageIndexRes.result) {
      using enum vk::Result;
      case eSuboptimalKHR:
      case eErrorOutOfDateKHR: {
        return;
      }
      case eSuccess: {
        break;
      }
      default: {
        // for debug lets just log
        logger->debug(std::to_string(static_cast<int>(imageIndexRes.result)));
      };
    }
    uint32_t imageIndex = imageIndexRes.value;

    auto& [renderComplete] = vulkanCtx.GetSwapchainSync(imageIndex);

    vulkanDevice->resetFences(*inFlight);


    // NOTE: Render

    // NOTE: Required to declare new frame for imgui
    imguiLayer->Begin();

    // We use a fixed dt for refresh callback or pass it in? 
    // For now, let's just use 0 or last dt if we want animations to continue.
    // However, refresh is usually for static content during drag.
    // layers.Update(dt); // dt is from outer scope

    layers.Render();
    layers.RecordCommands(imageIndex, currentFrame);

    imguiLayer->End();

    // NOTE: SUBMIT
    std::vector<vk::CommandBuffer> cmdBuffers = layers.GetCommandBuffers(currentFrame);


    vk::Semaphore waitSemaphores[] = {*imageAcquired};
    vk::PipelineStageFlags waitStages[] = {vk::PipelineStageFlagBits::eColorAttachmentOutput};
    vk::Semaphore signalSemaphores[] = {*renderComplete};


    vk::SubmitInfo submitInfo{};
    submitInfo.setWaitSemaphores(waitSemaphores);
    submitInfo.setWaitDstStageMask(waitStages);
    submitInfo.setCommandBuffers(cmdBuffers);
    submitInfo.setSignalSemaphores(signalSemaphores);

    CheckVulkanRes(vulkanCtx.GetGraphicsQueue().submit(submitInfo, *inFlight),
                   "Failed to submit to graphics queue");


    // NOTE: Present
    vulkanCtx.PresentImage(engineEventManager, imageIndex);

    currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
    frameCounter++;
  };

  window.SetRefreshCallback(renderFrame);

  // NOTE: MAIN LOOP

  bool isRunning = true;

  auto lastTime = std::chrono::high_resolution_clock::now();

  while (isRunning) {
    auto now = std::chrono::high_resolution_clock::now();
    float dt = std::chrono::duration<float>(now - lastTime).count();
    lastTime = now;

    isRunning = !window.ShouldClose();

    glfwPollEvents();

    // NOTE: Input Events
    std::ranges::for_each(inputEventManager,
                          [&](const auto& event) { layers.HandleEvent(*event); });
    inputEventManager.clear();

    layers.Update(dt);
    renderFrame();


    // NOTE: Performance stuff
    auto frameEnd = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = frameEnd - now;
    // TODO: Use a more robust frame pacing mechanism (e.g., vsync or a dedicated timer)
    std::chrono::duration<double> targetFrameTime(1.0 / TARGET_FPS);
    if (elapsed < targetFrameTime) {
      performanceLayer->BeginSleep();
      std::this_thread::sleep_for(targetFrameTime - elapsed);
      performanceLayer->EndSleep();
    }


    currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
    frameCounter++;
  }


  // cleanup
  CheckVulkanRes(vulkanDevice->waitIdle(), "Failed to wait for vulkandevice idle");


  g_dxcUtils.Release();
  g_dxcCompiler.Release();

  return 0;
}

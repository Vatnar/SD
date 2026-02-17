#pragma once
#include "Core/GlfwContext.hpp"
#include "Core/Vulkan/VulkanContext.hpp"
#include "Core/Vulkan/VulkanRenderer.hpp"

namespace SD {
class Application {
  GlfwContext mGlfwCtx;
  VulkanContext mVulkanCtx;
  VulkanRenderer mRenderer;

  std::unique_ptr<Scene> mGameScene{};
  std::unique_ptr<Scene> mEditorScene{};

  std::vector<std::unique_ptr<Window>> mWindows{};
  Window* mMainWindow;
  std::vector<std::unique_ptr<VulkanWindow>> mVulkanWindows{};

  EventManager mAppEventManager{};
  LayerList mNonVisualLayers{};


  bool mRunning = true;

public:
  Application();

  ~Application() = default;

  void RenderWindow(Window& window, VulkanWindow& vw);

  void Run();

  void OnAppEvent(Event& e);
};
} // namespace SD

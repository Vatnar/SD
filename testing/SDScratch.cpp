#include <Core/ECS/Entity.hpp>
#include <format>
#include <sstream>

#include "../SDEngine/include/Core/Vulkan/VulkanContext.hpp"
#include "Core/ECS/Component.hpp"
#include "Core/ECS/EntityManager.hpp"
#include "Core/Events/app/AppEvents.hpp"
#include "Core/Events/window/KeyboardEvents.hpp"
#include "Core/Events/window/WindowEvents.hpp"
#include "Core/GlfwContext.hpp"
#include "Core/Logging.hpp"
#include "Core/Scene.hpp"
#include "Core/Systems/Renderer/Material.hpp"
#include "Core/Systems/Renderer/Mesh.hpp"
#include "Core/Vulkan/VulkanRenderer.hpp"
#include "Core/Vulkan/VulkanWindow.hpp"
#include "Core/Window.hpp"
#include "Layers/DebugInfoLayer.hpp"
#include "Layers/ImGuiLayer.hpp"
#include "Layers/PerformanceLayer.hpp"
#include "Layers/Shader2DLayer.hpp"
#include "VLA/Matrix.hpp"


// struct StaticMeshRef {
//   std::shared_ptr<Mesh> mesh;
// };
// REGISTER_SD_COMPONENT(StaticMeshRef);
//
// struct MaterialRef {
//   std::shared_ptr<Material> material;
// };
// REGISTER_SD_COMPONENT(MaterialRef);
//
// struct Transform {
//   VLA::Matrix4x4f transform;
// };
// REGISTER_SD_COMPONENT(Transform);
//
// struct RigidBody {
//   VLA::Vector3f velocity;
//   VLA::Vector3f acceleration;
//   VLA::Vector3f force;
//   VLA::Vector3f moment;
// };
// REGISTER_SD_COMPONENT(RigidBody)
//
// struct Tag {
//   const char* tag = "hey";
// };
// REGISTER_SD_COMPONENT(Tag)
//
;


class Application {
  GlfwContext mGlfwCtx;
  VulkanContext mVulkanCtx;
  VulkanRenderer mRenderer;

  std::unique_ptr<Scene> mGameScene{};
  std::unique_ptr<Scene> mEditorScene{};

  std::vector<std::unique_ptr<Window>> mWindows{};
  Window* mMainWindow;
  std::vector<std::unique_ptr<VulkanWindow>> mVulkanWindows{};

  // App events
  EventManager mAppEventManager{};
  LayerList mNonVisualLayers{};


  bool mRunning = true;

public:
  Application() :
    mVulkanCtx(mGlfwCtx), mRenderer(mVulkanCtx), mMainWindow(nullptr), mVulkanWindows(),
    mAppEventManager(), mNonVisualLayers() {
    auto window = WindowBuilder().SetTitle("SDEngine").SetSize(1280, 720).Build();

    mVulkanCtx.Init(*window);

    auto vw = std::make_unique<VulkanWindow>(*window, mVulkanCtx);
    mVulkanWindows.push_back(std::move(vw));

    mWindows.push_back(std::move(window));
    mMainWindow = mWindows.front().get();

    // Scenes, TODO: make it possible to run these, while reloading layers
    mGameScene = std::make_unique<Scene>();

    // Layers
    std::vector<std::string> textures = {"assets/textures/CreateNoteInteraction.png",
                                         "assets/textures/example.jpg"};

      // Note: Passing *mVulkanWindows.front() (which is the unique_ptr content)
      mMainWindow->LayerStack.PushLayer<Shader2DLayer>(*mGameScene, mVulkanCtx, *mVulkanWindows.front(), textures);

    // dont need a window, just prints to terminal
    mNonVisualLayers.PushLayer<PerformanceLayer>(*mGameScene);


    mMainWindow->LayerStack.PushLayer<DebugInfoLayer>(*mGameScene);
    mMainWindow->LayerStack.PushLayer<ImGuiLayer>(*mGameScene, mVulkanCtx, *mMainWindow);

    for (auto& window : mWindows) {
      window->SetRefreshCallback([this, w = window.get()]() {
        for (size_t i = 0; i < mWindows.size(); ++i) {
          if (mWindows[i].get() == w) {
            RenderWindow(*mWindows[i], *mVulkanWindows[i]);
            break;
          }
        }
      });
    }
  }

  ~Application() = default;

  void RenderWindow(Window& window, VulkanWindow& vw) {
    if (vw.IsMinimized())
      return;

    // 1. Process Window Events (including Resize)
    for (auto& e : window.GetEventManager()) {
      EventDispatcher dispatcher(*e);

      dispatcher.Dispatch<WindowResizeEvent>([&](const WindowResizeEvent& event) {
        vw.Resize(event.width, event.height);
        return false; // pass to layers too
      });
      dispatcher.Dispatch<WindowCloseEvent>([&](const WindowCloseEvent& ev) {
        if (&window == mMainWindow) {
          mRunning = false;
        }
        return false;
      });

      // propagate events
      window.LayerStack.OnEvent(*e);
    }
    window.GetEventManager().Clear();

    // 2. Handle Resize (Immediate)
    if (vw.IsFramebufferResized()) {
      vw.RecreateSwapchain(window.LayerStack);
      vw.ResetFramebufferResized();
    }

    if (!mRunning)
      return;

    // 3. Acquire Image
    auto& frameSync = vw.GetFrameSync();
    auto [acquireResult, imageIndex] = vw.GetVulkanImages(frameSync.imageAcquired);

    if (acquireResult == vk::Result::eErrorOutOfDateKHR) {
      vw.RecreateSwapchain(window.LayerStack);
      return;
    }

    // 4. ImGui Begin
    if (auto* imguiLayer = window.LayerStack.Get<ImGuiLayer>()) {
      imguiLayer->Begin();
    }

    // 5. Update Layers
    for (auto& layer : window.LayerStack) {
      if (dynamic_cast<ImGuiLayer*>(layer.get()))
        continue;
      layer->OnUpdate(0.016f);
    }

    if (!mRunning)
      return;

    // 6. Record & Submit
    vk::CommandBuffer cmd = mRenderer.BeginFrame(vw);
    for (auto& layer : window.LayerStack) {
      layer->OnRender(cmd);
    }
    vk::Result presentRes = mRenderer.EndFrame(vw); // Submit & Present

    if (presentRes == vk::Result::eErrorOutOfDateKHR || presentRes == vk::Result::eSuboptimalKHR) {
      vw.RecreateSwapchain(window.LayerStack);
    }
  }

  void Run() {
    while (mRunning) {
      glfwPollEvents();

      for (auto& e : mAppEventManager) {
        OnAppEvent(*e);
      }
      mAppEventManager.Clear();

      for (auto& layer : mNonVisualLayers) {
        layer->OnUpdate(0.016f); // TODO: DeltaTime // fixed dt
      }

      // Process Windows
      for (size_t i = 0; i < mWindows.size(); ++i) {
        RenderWindow(*mWindows[i], *mVulkanWindows[i]);
      }
    }
  }

  void OnAppEvent(Event& e) {
    EventDispatcher dispatcher(e);

    dispatcher.Dispatch<AppTerminateEvent>([this](AppTerminateEvent& event) {
      mRunning = false;
      return true;
    });

    mNonVisualLayers.OnEvent(e);
  }
};

int main() {
  init_logging();

  Application app;
  app.Run();
}

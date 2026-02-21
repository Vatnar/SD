#pragma once

#include "Core/Base.hpp"
#include "Core/LayerList.hpp"
#include "Core/Window.hpp"
#include "Core/Vulkan/VulkanWindow.hpp"

#include <memory>
#include <unordered_map>
#include <vector>

namespace SD {

class ViewManager;
using WindowId = u32;

struct WindowProps {
  std::string title;
  int width, height;
  explicit WindowProps(const std::string& title = "SD Engine", int width = 1280, int height = 720) :
    title(title), width(width), height(height) {}
};

class WindowManager {
public:
  WindowManager();
  ~WindowManager();

  WindowId Create(const WindowProps& props);
  void Destroy(WindowId id);
  void ProcessPendingCloses();

  Window& GetWindow(WindowId id);
  VulkanWindow& GetRenderWindow(WindowId id);

  struct WindowData {
    std::unique_ptr<Window> logic;
    std::unique_ptr<VulkanWindow> render;
    LayerList viewLayers;

    WindowData() = default;
    WindowData(WindowData&&) = default;
    WindowData& operator=(WindowData&&) = default;
    WindowData(const WindowData&) = delete;
    WindowData& operator=(const WindowData&) = delete;
  };

  auto& GetWindows() { return mWindows; }
  const auto& GetWindows() const { return mWindows; }

  void UpdateWindows(float dt);
  void DrawWindows(ViewManager& viewManager);

private:
  void UpdateWindow(WindowId id, WindowData& data, float dt);
  void DrawWindow(WindowId id, WindowData& data, ViewManager& viewManager);

  std::unordered_map<WindowId, WindowData> mWindows;
  WindowId mNextWindowId = 0;
  std::vector<WindowId> mPendingClose;
};

} // namespace SD

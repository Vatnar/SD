#pragma once
#include <imgui.h>
#include <set>
#include <string>
#include <vector>

#include "Core/Events/window/KeyboardEvents.hpp"
#include "Core/Events/window/MouseEvents.hpp"
#include "Core/Layer.hpp"

// TODO: Consider making this a more generic "DebugUI" layer that can display various debug
// information, not just input
// TODO: REMOVE
using namespace SD;
class DebugInfoLayer : public Layer {
public:
  DebugInfoLayer(Scene& scene) : Layer(scene) {};

  void OnAttach() override {}

  void OnDetach() override {}

  void OnUpdate(float dt) override {}

  void OnRender(vk::CommandBuffer& cmd) override {
    ImGui::Begin("Debug Info");

    ImGui::Text("Mouse Position: (%.1f, %.1f)", mMouseX, mMouseY);

    ImGui::Text("Scroll Offset: (%.1f, %.1f)", mScrollX, mScrollY);

    if (mPressedKeys.empty()) {
      ImGui::Text("Keys Pressed: None");
    } else {
      ImGui::Text("Keys Pressed:");
      for (int key : mPressedKeys) {
        ImGui::SameLine();
        ImGui::Text("%d", key);
      }
    }

    if (mPressedMouseButtons.empty()) {
      ImGui::Text("Mouse Buttons Pressed: None");
    } else {
      ImGui::Text("Mouse Buttons Pressed:");
      for (int button : mPressedMouseButtons) {
        ImGui::SameLine();
        ImGui::Text("%d", button);
      }
    }

    ImGui::End();
  }

  void OnEvent(Event& e) override {
    switch (e.GetEventType()) {
      case EventType::MouseMoved: {
        auto& cursorEvent = dynamic_cast<MouseMovedEvent&>(e);
        mMouseX = cursorEvent.xPos;
        mMouseY = cursorEvent.yPos;
        break;
      }
      case EventType::MouseScrolled: {
        auto& scrollEvent = dynamic_cast<MouseScrolledEvent&>(e);
        mScrollX += scrollEvent.xOffset;
        mScrollY += scrollEvent.yOffset;
        break;
      }
      case EventType::KeyPressed: {
        auto& keyEvent = dynamic_cast<KeyPressedEvent&>(e);
        if (!keyEvent.repeat) {
          mPressedKeys.insert(keyEvent.key);
        }
        break;
      }
      case EventType::KeyReleased: {
        auto& keyEvent = dynamic_cast<KeyReleasedEvent&>(e);
        mPressedKeys.erase(keyEvent.key);
        break;
      }
      case EventType::MousePressed: {
        auto& mouseEvent = dynamic_cast<MousePressedEvent&>(e);
        mPressedMouseButtons.insert(mouseEvent.button);
        break;
      }
      case EventType::MouseReleased: {
        auto& mouseEvent = dynamic_cast<MouseReleasedEvent&>(e);
        mPressedMouseButtons.erase(mouseEvent.button);
        break;
      }
      default:
        break;
    }
  }

private:
  double mMouseX = 0.0;
  double mMouseY = 0.0;
  double mScrollX = 0.0;
  double mScrollY = 0.0;
  std::set<int> mPressedKeys;
  std::set<int> mPressedMouseButtons;
};

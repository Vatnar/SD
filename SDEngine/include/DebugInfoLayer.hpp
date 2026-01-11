#pragma once
#include "Layer.hpp"
#include "Event.hpp"
#include "imgui.h"
#include <string>
#include <vector>
#include <set>

class DebugInfoLayer : public Layer
{
public:
    DebugInfoLayer() = default;

    void OnAttach() override {}

    void OnDetach() override {}

    void OnUpdate(float dt) override {}

    void OnRender() override
    {
        ImGui::Begin("Debug Info");

        ImGui::Text("Mouse Position: (%.1f, %.1f)", mMouseX, mMouseY);

        ImGui::Text("Scroll Offset: (%.1f, %.1f)", mScrollX, mScrollY);

        if (mPressedKeys.empty())
        {
            ImGui::Text("Keys Pressed: None");
        }
        else
        {
            ImGui::Text("Keys Pressed:");
            for (int key : mPressedKeys)
            {
                ImGui::SameLine();
                ImGui::Text("%d", key);
            }
        }

        if (mPressedMouseButtons.empty())
        {
            ImGui::Text("Mouse Buttons Pressed: None");
        }
        else
        {
            ImGui::Text("Mouse Buttons Pressed:");
            for (int button : mPressedMouseButtons)
            {
                ImGui::SameLine();
                ImGui::Text("%d", button);
            }
        }

        ImGui::End();
    }

    void OnEvent(Event& e) override
    {
        switch (e.category())
        {
            case EventCategory::CursorPos:
            {
                auto& cursorEvent = dynamic_cast<CursorEvent&>(e);
                mMouseX           = cursorEvent.xPos;
                mMouseY           = cursorEvent.yPos;
                break;
            }
            case EventCategory::MouseScroll:
            {
                auto& scrollEvent = dynamic_cast<ScrollEvent&>(e);
                mScrollX += scrollEvent.xOffset;
                mScrollY += scrollEvent.yOffset;
                break;
            }
            case EventCategory::KeyPressed:
            {
                auto& keyEvent = dynamic_cast<KeyPressedEvent&>(e);
                if (!keyEvent.repeat)
                {
                    mPressedKeys.insert(keyEvent.key);
                }
                break;
            }
            case EventCategory::KeyReleased:
            {
                auto& keyEvent = dynamic_cast<KeyReleasedEvent&>(e);
                mPressedKeys.erase(keyEvent.key);
                break;
            }
            case EventCategory::MousePressed:
            {
                auto& mouseEvent = dynamic_cast<MousePressedEvent&>(e);
                mPressedMouseButtons.insert(mouseEvent.button);
                break;
            }
            case EventCategory::MouseReleased:
            {
                auto& mouseEvent = dynamic_cast<MouseReleasedEvent&>(e);
                mPressedMouseButtons.erase(mouseEvent.button);
                break;
            }
            default:
                break;
        }
    }

private:
    double        mMouseX  = 0.0;
    double        mMouseY  = 0.0;
    double        mScrollX = 0.0;
    double        mScrollY = 0.0;
    std::set<int> mPressedKeys;
    std::set<int> mPressedMouseButtons;
};

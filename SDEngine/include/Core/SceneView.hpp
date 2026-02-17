#pragma once

#include "Core/View.hpp"
#include "Core/SDImGuiViewport.hpp"

namespace SD {

class SceneView : public View {
public:
    SceneView(const std::string& name, VulkanContext& ctx);
    virtual ~SceneView() override = default;

    virtual void OnGuiRender() override;
    void OnRender(vk::CommandBuffer cmd);

    SDImGuiViewport& GetViewport() { return mViewport; }

private:
    SDImGuiViewport mViewport;
};

} // namespace SD

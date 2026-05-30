#pragma once

#include "core/View.hpp"
#include "core/SDImGuiViewport.hpp"

namespace sd {

class SceneView : public View {
public:
    SceneView(const std::string& name, VulkanContext& ctx);
    virtual ~SceneView() override = default;

    virtual void on_gui_render() override;
    void on_render(vk::CommandBuffer cmd) override;

    SDImGuiViewport& get_viewport() { return m_viewport; }

private:
    SDImGuiViewport m_viewport;
};

} // namespace SD

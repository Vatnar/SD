#pragma once
#include <SD/core/Layer.hpp>

class GameRenderLayer : public sd::RenderStage {
  vk::UniquePipeline       m_pipeline;
  vk::UniquePipeline       m_wireframe;
  vk::UniquePipelineLayout m_layout;

public:
  GameRenderLayer(const std::string&       name,
                  sd::Scene*               scene,
                  vk::UniquePipeline       pipeline,
                  vk::UniquePipelineLayout layout);
  void on_render(vk::CommandBuffer cmd) override;
};

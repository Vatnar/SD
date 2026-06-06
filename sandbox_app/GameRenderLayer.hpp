#pragma once
#include <SD/core/Layer.hpp>

class GameRenderLayer : public sd::RenderStage {
  VkPipeline       m_pipeline;
  VkPipeline       m_wireframe;
  VkPipelineLayout m_layout;

public:
  GameRenderLayer(const std::string& name,
                  sd::Scene*         scene,
                  vk::Pipeline       pipeline,
                  vk::Pipeline       wireframe,
                  vk::PipelineLayout layout);
  void on_render(vk::CommandBuffer cmd) override;
};

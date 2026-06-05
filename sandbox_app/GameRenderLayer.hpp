#pragma once
#include <SD/core/Layer.hpp>

class GameRenderLayer : public sd::RenderStage {
  VkPipeline       m_pipeline;
  VkPipeline       m_wireframe;
  VkPipelineLayout m_layout;

public:
  GameRenderLayer(const std::string& name, sd::Scene* scene, VkPipeline pipeline,
                  VkPipeline wireframe, VkPipelineLayout layout);
  void on_render(vk::CommandBuffer cmd) override;
};

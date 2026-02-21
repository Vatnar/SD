#pragma once
#include "Core/Layer.hpp"

class GameRenderLayer : public SD::RenderStage {
  VkPipeline mPipeline;
  VkPipeline mWireframePipeline;
  VkPipelineLayout mLayout;

public:
  GameRenderLayer(const std::string& name, SD::Scene* scene, VkPipeline pipeline,
                  VkPipeline mWireframePipeline, VkPipelineLayout layout);
  void OnRender(vk::CommandBuffer cmd) override;
};

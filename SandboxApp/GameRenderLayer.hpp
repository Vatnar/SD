#pragma once
#include "Core/Layer.hpp"
#include "Core/Vulkan/PipelineFactory.hpp"

class GameRenderLayer : public SD::RenderStage {
  SD::PipelineFactory::Handle mPipelineHandle;
  SD::PipelineFactory::Handle mWireframeHandle;
  VkPipelineLayout mLayout;
  SD::PipelineFactory* mPipelineFactory;

public:
  GameRenderLayer(const std::string& name, SD::Scene* scene, 
                  SD::PipelineFactory::Handle pipelineHandle,
                  SD::PipelineFactory::Handle wireframeHandle,
                  VkPipelineLayout layout, SD::PipelineFactory* pipelineFactory);
  void OnRender(vk::CommandBuffer cmd) override;
};

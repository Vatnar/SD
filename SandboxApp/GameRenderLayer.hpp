#pragma once
#include "core/Layer.hpp"
#include "core/vulkan/PipelineFactory.hpp"

class GameRenderLayer : public sd::RenderStage {
  sd::PipelineFactory::Handle m_pipeline_handle;
  sd::PipelineFactory::Handle m_wireframe_handle;
  VkPipelineLayout m_layout;
  sd::PipelineFactory* m_pipeline_factory;

public:
  GameRenderLayer(const std::string& name, sd::Scene* scene,
                  sd::PipelineFactory::Handle pipeline_handle,
                  sd::PipelineFactory::Handle wireframe_handle,
                  VkPipelineLayout layout, sd::PipelineFactory* pipeline_factory);
  void on_render(vk::CommandBuffer cmd) override;
};

#pragma once
#include <SD/core/Layer.hpp>

class GameRenderLayer : public sd::RenderStage {
  vk::UniquePipeline       m_pipeline;
  vk::UniquePipeline       m_wireframe;
  vk::UniquePipelineLayout m_layout;
  vk::UniqueBuffer         m_vertex_buffer;
  vk::UniqueDeviceMemory   m_vertex_memory;
  vk::DeviceSize           m_vertex_count   = 0;
  bool                     m_buffer_created = false;

  void create_vertex_buffer();

public:
  GameRenderLayer(const std::string&       name,
                  sd::Scene*               scene,
                  vk::UniquePipeline       pipeline,
                  vk::UniquePipelineLayout layout);
  void on_render(vk::CommandBuffer cmd) override;
};

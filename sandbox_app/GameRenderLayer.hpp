#pragma once
#include <SD/core/Layer.hpp>
#include <vulkan/vulkan.hpp>

class GameRenderLayer : public sd::RenderStage {
  vk::UniquePipeline       m_pipeline;
  vk::UniquePipeline       m_wireframe;
  vk::UniquePipelineLayout m_layout;
  vk::UniqueBuffer         m_vertex_buffer;
  vk::UniqueDeviceMemory   m_vertex_memory;
  vk::DeviceSize           m_vertex_count   = 0;
  bool                     m_buffer_created = false;

  const char*     m_vert_path    = nullptr;
  const char*     m_frag_path    = nullptr;
  vk::PolygonMode m_polygon_mode = {};

  void create_vertex_buffer();

public:
  void reload_shaders();
  GameRenderLayer(const char*              name,
                  sd::Scene*               scene,
                  vk::UniquePipeline       pipeline,
                  vk::UniquePipelineLayout layout,
                  const char*              vert_path    = nullptr,
                  const char*              frag_path    = nullptr,
                  vk::PolygonMode          polygon_mode = {});
  void on_render(vk::CommandBuffer cmd);
  void on_shader_reload();
};

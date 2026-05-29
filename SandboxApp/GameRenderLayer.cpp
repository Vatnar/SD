#include "GameRenderLayer.hpp"

#include "Core/ECS/Components.hpp"
#include "Core/View.hpp"
#include "Utils/Utils.hpp"
#include "VLA/Matrix.hpp"
#include "imgui.h"

GameRenderLayer::GameRenderLayer(const std::string& name, sd::Scene* scene,
                                 sd::PipelineFactory::Handle pipeline_handle,
                                 sd::PipelineFactory::Handle wireframe_handle,
                                 VkPipelineLayout layout, sd::PipelineFactory* pipeline_factory) :
  RenderStage(name, scene), m_pipeline_handle(pipeline_handle), m_wireframe_handle(wireframe_handle),
  m_layout(layout), m_pipeline_factory(pipeline_factory) {
}

void GameRenderLayer::on_render(vk::CommandBuffer cmd) {
  SD_DEBUG_ASSERT(m_pipeline_factory, "PipelineFactory must be valid");

  // Resolve handles to actual pipelines (stable across hot-reloads)
  VkPipeline active_pipe = (m_view->get_render_mode() == sd::RenderMode::WIREFRAME)
                              ? m_pipeline_factory->get_pipeline(m_wireframe_handle)
                              : m_pipeline_factory->get_pipeline(m_pipeline_handle);

  SD_DEBUG_ASSERT(cmd != VK_NULL_HANDLE, "Invalid cmd buffer");
  SD_DEBUG_ASSERT(active_pipe != VK_NULL_HANDLE, "Invalid pipeline - was it destroyed or recreated?");
  SD_DEBUG_ASSERT(m_layout != VK_NULL_HANDLE, "Invalid layout");

  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, active_pipe);

  struct Push {
    VLA::Matrix4x4f mvp;
    float color[4];
  };

  auto mouse_pos = ImGui::GetMousePos();
  auto region_pos = m_view->get_content_region_pos();
  auto region_extent = m_view->get_content_region_extent();
  float mouse_ndc_x = ((mouse_pos.x - region_pos.x) / region_extent.x) * 2.0f - 1.0f;
  float mouse_ndc_y = ((mouse_pos.y - region_pos.y) / region_extent.y) * 2.0f - 1.0f;

  // Triangle vertices in local space (from world.vert)
  const VLA::Vector2f v0{
      {0.0f, -0.8f}
  };
  const VLA::Vector2f v1{
      {-0.8f, 0.8f}
  };
  const VLA::Vector2f v2{
      {0.8f, 0.8f}
  };

  float aspect =
      static_cast<float>(m_view->get_extent().width) / static_cast<float>(m_view->get_extent().height);
  float scale_x = (aspect > 1.0f) ? 1.0f / aspect : 1.0f;
  float scale_y = (aspect > 1.0f) ? 1.0f : aspect; // Correcting for FixedWidth if aspect < 1

  auto to_ndc = [&](VLA::Vector2f v) {
    VLA::Vector2f res;
    res[0] = v[0] * scale_x;
    res[1] = v[1] * scale_y;
    return res;
  };

  const VLA::Vector2f n0 = to_ndc(v0);
  const VLA::Vector2f n1 = to_ndc(v1);
  const VLA::Vector2f n2 = to_ndc(v2);
  const VLA::Vector2f p{
      {mouse_ndc_x, mouse_ndc_y}
  };

  auto cross = [](VLA::Vector2f a, VLA::Vector2f b) {
    return a[0] * b[1] - a[1] * b[0];
  };
  const float d1 = cross(p - n0, n1 - n0);
  const float d2 = cross(p - n1, n2 - n1);
  const float d3 = cross(p - n2, n0 - n2);

  const bool inside = !((d1 < 0 || d2 < 0 || d3 < 0) && (d1 > 0 || d2 > 0 || d3 > 0));

  for (auto [entity, transform, renderable] : m_scene->em.view<sd::Transform, sd::Renderable>()) {
    if ((renderable.view_mask & 1u << static_cast<uint32_t>(m_view_id)) == 0 || renderable.render_stage != m_stage_id)
      continue;

    Push push;
    push.mvp = m_view->get_projection() * transform.world_matrix;
    memcpy(push.color, renderable.color, sizeof(float) * 4);

    if (inside) {
      push.color[0] = 1.0f;
      push.color[1] = 1.0f;
      push.color[2] = 1.0f;
    }

    vkCmdPushConstants(cmd, m_layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(Push), &push);

    vkCmdDraw(cmd, 3, 1, 0, 0);
  }
}

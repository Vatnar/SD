#include "GameRenderLayer.hpp"

#include <cstring>
#include <imgui.h>
#include <ranges>
#include <utility>

#include <SD/Application.hpp>
#include <SD/core/View.hpp>
#include <SD/core/ecs/components.hpp>
#include <SD/utils/utils.hpp>
#include <VLA/Matrix.hpp>

#include "SD/Vertex.hpp"
#include "SD/profiler.hpp"
#include "logging.hpp"

namespace game {
std::pair<vk::UniquePipeline, vk::UniquePipelineLayout>
create_pipeline(vk::Device, const char*, const char*, vk::PolygonMode, sd::View*);
} // namespace game

GameRenderLayer::GameRenderLayer(const char*              name,
                                 sd::Scene*               scene,
                                 vk::UniquePipeline       pipeline,
                                 vk::UniquePipelineLayout layout,
                                 const char*              vert_path,
                                 const char*              frag_path,
                                 vk::PolygonMode          polygon_mode) :
  m_pipeline(std::move(pipeline)), m_layout(std::move(layout)), m_vert_path(vert_path),
  m_frag_path(frag_path), m_polygon_mode(polygon_mode) {
  debug_name  = name;
  this->scene = scene;
}

void GameRenderLayer::reload_shaders() {
  auto& device{*app->services().vulkan.get_vulkan_device()};
  (void)device.waitIdle();

  auto [new_pipeline,
        new_layout]{game::create_pipeline(device, m_vert_path, m_frag_path, m_polygon_mode, view)};

  if (new_pipeline && new_layout) {
    m_pipeline = std::move(new_pipeline);
    m_layout   = std::move(new_layout);
  } else {
    sd::log::game::error("Shader reload failed — keeping old pipeline");
  }
}

void GameRenderLayer::on_shader_reload() {
  reload_shaders();
}

void GameRenderLayer::create_vertex_buffer() {
  constexpr std::array<sd::VertexPNUV, 3> triangle{
      {
       {.position = {0.0f, -0.8f, 0.0f}, .normal = {0.0f, 0.0f, 1.0f}, .uv = {0.5f, 0.0f}},
       {.position = {-0.8f, 0.8f, 0.0f}, .normal = {0.0f, 0.0f, 1.0f}, .uv = {0.0f, 1.0f}},
       {.position = {0.8f, 0.8f, 0.0f}, .normal = {0.0f, 0.0f, 1.0f}, .uv = {1.0f, 1.0f}},
       }
  };
  constexpr vk::DeviceSize buffer_size{sizeof(triangle)};

  const vk::Device&         device{*app->services().vulkan.get_vulkan_device()};
  const vk::PhysicalDevice& phys_dev{app->services().vulkan.get_physical_device()};

  // TODO: staging upload to device local memory
  auto [vb, vm]{sd::create_buffer(
      device,
      phys_dev,
      buffer_size,
      vk::BufferUsageFlagBits::eVertexBuffer,
      vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent)};

  void* mapped{sd::check_vulkan_res_val(device.mapMemory(*vm, 0, buffer_size),
                                        "Failed to map vertex buffer memory")};

  std::memcpy(mapped, triangle.data(), static_cast<size_t>(buffer_size));
  device.unmapMemory(*vm);

  m_vertex_buffer  = std::move(vb);
  m_vertex_memory  = std::move(vm);
  m_vertex_count   = triangle.size();
  m_buffer_created = true;
}

void GameRenderLayer::on_render(vk::CommandBuffer cmd) {
  if (!m_buffer_created) {
    create_vertex_buffer();
  }

  ASSERT(cmd != VK_NULL_HANDLE && "Invalid cmd buffer");
  ASSERT(m_pipeline && "Invalid pipeline - was it destroyed or recreated?");
  ASSERT(m_layout && "Invalid layout");

  vk::Extent2D extent{view->get_extent()};

  vk::RenderingAttachmentInfo color_attachment{
      .imageView   = view->get_color_view(),
      .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
      .loadOp      = vk::AttachmentLoadOp::eClear,
      .storeOp     = vk::AttachmentStoreOp::eStore,
      .clearValue =
          vk::ClearValue{.color = vk::ClearColorValue{std::array{0.1f, 0.1f, 0.1f, 1.0f}}},
  };

  vk::RenderingAttachmentInfo depth_attachment{
      .imageView   = view->get_depth_view(),
      .imageLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal,
      .loadOp      = vk::AttachmentLoadOp::eClear,
      .storeOp     = vk::AttachmentStoreOp::eDontCare,
      .clearValue  = vk::ClearValue{.depthStencil = vk::ClearDepthStencilValue{1.0f, 0}},
  };

  vk::RenderingInfo rendering_info{
      .renderArea           = vk::Rect2D{.offset = {0, 0}, .extent = extent},
      .layerCount           = 1,
      .colorAttachmentCount = 1,
      .pColorAttachments    = &color_attachment,
      .pDepthAttachment     = &depth_attachment,
  };

  cmd.beginRendering(rendering_info);

  vk::Viewport viewport{.x        = 0,
                        .y        = 0,
                        .width    = static_cast<F32>(extent.width),
                        .height   = static_cast<F32>(extent.height),
                        .minDepth = 0,
                        .maxDepth = 1};
  cmd.setViewport(0, viewport);
  cmd.setScissor(0,
                 vk::Rect2D{
                     {0, 0},
                     extent
  });

  cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, *m_pipeline);

  struct Push {
    VLA::Matrix4x4f mvp{};
    // F32             color[4]{};
    VLA::Vector4f color{};
  };

  ImVec2 mouse_pos{ImGui::GetMousePos()};
  ImVec2 region_pos{view->get_content_region_pos()};
  ImVec2 region_extent{view->get_content_region_extent()};
  F32    mouse_ndc_x{((mouse_pos.x - region_pos.x) / region_extent.x) * 2.0f - 1.0f};
  F32    mouse_ndc_y{((mouse_pos.y - region_pos.y) / region_extent.y) * 2.0f - 1.0f};

  // Triangle vertices in local space (from world.vert)
  constexpr VLA::Vector2f v0{
      {0.0f, -0.8f}
  };
  constexpr VLA::Vector2f v1{
      {-0.8f, 0.8f}
  };
  constexpr VLA::Vector2f v2{
      {0.8f, 0.8f}
  };

  F32 aspect{static_cast<F32>(view->get_extent().width) /
             static_cast<F32>(view->get_extent().height)};
  F32 scale_x{(aspect > 1.0f) ? 1.0f / aspect : 1.0f};
  F32 scale_y{(aspect > 1.0f) ? 1.0f : aspect}; // Correcting for FixedWidth if aspect < 1

  auto to_ndc{[&](VLA::Vector2f v) {
    VLA::Vector2f res;
    res[0] = v[0] * scale_x;
    res[1] = v[1] * scale_y;
    return res;
  }};


  const VLA::Vector2f n0{to_ndc(v0)};
  const VLA::Vector2f n1{to_ndc(v1)};
  const VLA::Vector2f n2{to_ndc(v2)};
  // TODO: check if we can use to_ndc lambda here
  const VLA::Vector2f p{
      {mouse_ndc_x, mouse_ndc_y}
  };

  auto cross{[](VLA::Vector2f a, VLA::Vector2f b) { return a[0] * b[1] - a[1] * b[0]; }};
  // VLA::Vector2f::Cross(p - n0, n1 - n0);
  const F32 d1{cross(p - n0, n1 - n0)};
  const F32 d2{cross(p - n1, n2 - n1)};
  const F32 d3{cross(p - n2, n0 - n2)};

  // Just checks for equilateral triangle
  const bool inside{!((d1 < 0 || d2 < 0 || d3 < 0) && (d1 > 0 || d2 > 0 || d3 > 0))};

  // todo: should be on a mesh component, (or not on the component, but indexed to or something)

  for (auto [entity, transform, renderable] :
       scene->em.view<sd::components::Transform, sd::components::Renderable>()) {
    if ((renderable.view_mask & 1u << static_cast<uint32_t>(view_id)) == 0 ||
        renderable.render_stage != stage_id)
      continue;

    Push push{
        .mvp = view->get_projection() * transform.world_matrix,
    };
    memcpy(&push.color, renderable.color, sizeof(F32) * 4);


    if (inside) {
      push.color[0] = 1.0f;
      push.color[1] = 1.0f;
      push.color[2] = 1.0f;
    }


    cmd.pushConstants(*m_layout, vk::ShaderStageFlagBits::eVertex, 0, sizeof(Push), &push);
    const vk::Buffer     vertex_buffer{*m_vertex_buffer};
    const vk::DeviceSize offset{};

    cmd.bindVertexBuffers(0, 1, &vertex_buffer, &offset);

    cmd.draw(m_vertex_count, 1, 0, 0);
  }

  cmd.endRendering();
}

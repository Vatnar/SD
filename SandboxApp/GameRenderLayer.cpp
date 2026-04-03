#include "GameRenderLayer.hpp"

#include "Core/ECS/Components.hpp"
#include "Core/View.hpp"
#include "Utils/Utils.hpp"
#include "VLA/Matrix.hpp"
#include "imgui.h"

// static int sLoadCount = 0;

GameRenderLayer::GameRenderLayer(const std::string& name, SD::Scene* scene, VkPipeline pipeline,
                                 VkPipeline wireframePipeline, VkPipelineLayout layout) :
  RenderStage(name, scene), mPipeline(pipeline), mWireframePipeline(wireframePipeline),
  mLayout(layout) {
}

void GameRenderLayer::OnRender(vk::CommandBuffer cmd) {
  VkPipeline activePipe =
      (mView->GetRenderMode() == SD::RenderMode::Wireframe) ? mWireframePipeline : mPipeline;
  // BUG: Crash here SEGFAULT
  SD_CORE_ASSERT(cmd != VK_NULL_HANDLE, "Invalid cmd buffer");
  SD_CORE_ASSERT(activePipe != VK_NULL_HANDLE, "Invalid pipeline");
  SD_CORE_ASSERT(mLayout != VK_NULL_HANDLE, "Invalid layout");

  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, activePipe);

  struct Push {
    VLA::Matrix4x4f mvp;
    float color[4];
  };

  auto mousePos = ImGui::GetMousePos();
  auto regionPos = mView->GetContentRegionPos();
  auto regionExtent = mView->GetContentRegionExtent();
  float mouseNdcX = ((mousePos.x - regionPos.x) / regionExtent.x) * 2.0f - 1.0f;
  float mouseNdcY = ((mousePos.y - regionPos.y) / regionExtent.y) * 2.0f - 1.0f;

  // Triangle vertices in local space (from world.vert)
  VLA::Vector2f v0{
      {0.0f, -0.8f}
  };
  VLA::Vector2f v1{
      {-0.8f, 0.8f}
  };
  VLA::Vector2f v2{
      {0.8f, 0.8f}
  };

  float aspect =
      static_cast<float>(mView->GetExtent().width) / static_cast<float>(mView->GetExtent().height);
  float scaleX = (aspect > 1.0f) ? 1.0f / aspect : 1.0f;
  float scaleY = (aspect > 1.0f) ? 1.0f : aspect; // Correcting for FixedWidth if aspect < 1

  auto toNdc = [&](VLA::Vector2f v) {
    VLA::Vector2f res;
    res[0] = v[0] * scaleX;
    res[1] = v[1] * scaleY;
    return res;
  };

  VLA::Vector2f n0 = toNdc(v0);
  VLA::Vector2f n1 = toNdc(v1);
  VLA::Vector2f n2 = toNdc(v2);
  VLA::Vector2f p{
      {mouseNdcX, mouseNdcY}
  };

  auto cross = [](VLA::Vector2f a, VLA::Vector2f b) {
    return a[0] * b[1] - a[1] * b[0];
  };
  float d1 = cross(p - n0, n1 - n0);
  float d2 = cross(p - n1, n2 - n1);
  float d3 = cross(p - n2, n0 - n2);

  bool inside = !((d1 < 0 || d2 < 0 || d3 < 0) && (d1 > 0 || d2 > 0 || d3 > 0));

  for (auto [entity, transform, renderable] : mScene->em.View<SD::Transform, SD::Renderable>()) {
    if ((renderable.viewMask & 1u << mViewId) == 0 || renderable.renderStage != mStageId)
      continue;

    Push push;
    push.mvp = mView->GetProjection() * transform.worldMatrix;
    memcpy(push.color, renderable.color, sizeof(float) * 4);

    if (inside) {
      push.color[0] = 1.0f;
      push.color[1] = 0.0f;
      push.color[2] = 1.0f;
    }

    vkCmdPushConstants(cmd, mLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(Push), &push);

    vkCmdDraw(cmd, 3, 1, 0, 0);
  }
}

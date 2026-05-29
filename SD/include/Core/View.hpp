// TODO(docs): Add file-level Doxygen header
//   - @file View.hpp
//   - @brief View class - represents a renderable viewport/window content
//   - Relationship to Layer, LayerList, and VulkanWindow
//   - Aspect ratio and render mode handling
#pragma once

#include <imgui.h>
#include <memory>
#include <string>
#include <vector>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.hpp>

#include "Core/IdTypes.hpp"
#include "Core/LayerList.hpp"
#include "VLA/Matrix.hpp"

namespace sd {

// TODO(docs): Document ViewError enum
//   - Each error code's meaning
//   - How errors are propagated (std::expected)
enum ViewError {
  NAME_ALREADY_EXISTS,
  SUCCESS,
  VIEW_DOES_NOT_EXIST,
};

// TODO(docs): Document AspectMode enum
//   - Each mode's behavior
//   - When to use which mode
enum class AspectMode {
  FIXED_HEIGHT, // -aspect to +aspect
  FIXED_WIDTH,  // -1 to +1
  BEST_FIT      // Auto switch based on aspect
};

// TODO(docs): Document RenderMode enum
enum class RenderMode {
  SHADED,
  WIREFRAME
};

// TODO(docs): Document View class thoroughly
//   - Purpose: A renderable region with its own layers and Vulkan resources
//   - Ownership model (layers, Vulkan resources)
//   - Integration with ImGui for viewport windows
//   - Aspect ratio management
//   - Render pass and framebuffer management
//   - Example: Creating a custom view with layers
class View {
public:
  explicit View(const std::string& name) : m_name(name) {
    m_camera_view_projection = VLA::Matrix4x4f::Ortho(-1.0f, 1.0f, -1.0f, 1.0f, -1.0f, 1.0f);
  }
  virtual ~View();

  void on_update(float dt) { m_layers.update(dt); }
  virtual void on_gui_render();

  virtual void on_event(Event& e) { m_layers.on_event(e); }
  virtual void on_render(vk::CommandBuffer cmd);
  virtual void on_fixed_update(double dt) { m_layers.on_fixed_update(dt); }

  // --- Layer management ---
  template<typename T, typename... Args>
    requires std::is_base_of_v<Layer, T>
  T& push_layer(Args&&... args) {
    return m_layers.push_layer<T>(std::forward<Args>(args)...);
  }

  template<typename T, typename... Args>
    requires std::is_base_of_v<Layer, T>
  T& push_layer(int stageOrder, Args&&... args) {
    auto layer = std::make_unique<T>(std::forward<Args>(args)...);
    layer->m_stage_id = stageOrder;
    layer->m_view_id = m_view_id;
    layer->m_view = this;

    if (static_cast<size_t>(stageOrder) >= m_layers_by_stage.size())
      m_layers_by_stage.resize(stageOrder + 1);
    m_layers_by_stage[stageOrder] = std::move(layer);
    return static_cast<T&>(*m_layers_by_stage[stageOrder]);
  }

  // --- Identity ---
  const std::string& get_name() const { return m_name; }
  [[nodiscard]] ViewId get_view_id() const { return m_view_id; }
  [[nodiscard]] bool is_open() const { return m_open; }
  void SetOpen(bool open) { m_open = open; }

  // --- Layers ---
  LayerList& get_layers() { return m_layers; }
  const LayerList& get_layers() const { return m_layers; }

  // --- Viewport state ---
  VkExtent2D get_extent() const { return m_extent; }
  void resize(VkExtent2D extent);

  AspectMode get_aspect_mode() const { return m_aspect_mode; }
  void set_aspect_mode(AspectMode mode) {
    m_aspect_mode = mode;
    resize(m_extent);
  }

  RenderMode get_render_mode() const { return m_render_mode; }
  void set_render_mode(RenderMode mode) { m_render_mode = mode; }

  const VLA::Matrix4x4f& get_projection() const { return m_camera_view_projection; }

  /// Returns true (once) if the extent changed since the last call.
  bool consume_extent_changed() {
    bool changed = m_extent_changed;
    m_extent_changed = false;
    return changed;
  }

  // --- Diagnostics (ImGui region tracking) ---
  ImVec2 get_content_region_pos() const { return m_content_region_pos; }
  ImVec2 get_content_region_extent() const { return m_content_region_extent; }

  // --- Rendering setup ---
  static VkExtent2D get_im_gui_extent();
  static VkFormat find_depth_format();
  [[nodiscard]] vk::RenderPass get_layered_render_pass() const { return m_layered_rp.get(); }
  Layer* get_layer_by_stage(u32 stage);

  void setup_layered_render(u32 maxStages, VkExtent2D initialExtent = {1280, 720});
  void cleanup_layered_render();

private:
  void create_vulkan_resources();

  std::string m_name;
  LayerList m_layers;
  bool m_open = true;
  ViewId m_view_id;

  // ImGui content region (updated each frame in OnGuiRender)
  ImVec2 m_content_region_pos{0, 0};
  ImVec2 m_content_region_extent{0, 0};

  // Vulkan rendering resources
  vk::UniqueRenderPass m_layered_rp;

  VkImage m_color_image = VK_NULL_HANDLE;
  VmaAllocation m_color_allocation = VK_NULL_HANDLE;
  vk::UniqueImageView m_color_view;

  VkImage m_depth_image = VK_NULL_HANDLE;
  VmaAllocation m_depth_allocation = VK_NULL_HANDLE;
  vk::UniqueImageView m_depth_view;

  VkDescriptorSet m_display_tex_ds = VK_NULL_HANDLE; // imgui::image

  vk::UniqueFramebuffer m_layered_framebuffer;

  std::vector<std::unique_ptr<Layer>> m_layers_by_stage;
  VLA::Matrix4x4f m_camera_view_projection;
  VkExtent2D m_extent = {1280, 720};
  bool m_extent_changed = false;

  AspectMode m_aspect_mode = AspectMode::BEST_FIT;
  RenderMode m_render_mode = RenderMode::SHADED;

  friend class Application; // needs mViewId assignment
  friend class ViewManager;
};


} // namespace SD

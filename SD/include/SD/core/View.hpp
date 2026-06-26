// TODO(docs): Add file-level Doxygen header
//   - @file View.hpp
//   - @brief View class - represents a renderable viewport/window content
//   - Relationship to Layer, LayerList, and VulkanWindow
//   - Aspect ratio and render mode handling
#pragma once

#include <imgui.h>
#include <string>
#include <vector>
#include <vk_mem_alloc.h>

#include "SD/core/EngineServices.hpp"
#include "SD/core/LayerList.hpp"
#include "SD/core/id_types.hpp"
#include "SD/core/vulkan/VulkanContext.hpp"
#include "SD/core/vulkan/vulkan_config.hpp"
#include "VLA/Matrix.hpp"

namespace sd {

struct Application;

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
struct SD_EXPORT View {
  View(const std::string& name, const EngineServices& services) :
    m_name(name), m_view_id(0), m_vulkan_ctx(services.vulkan), m_imgui_ctx(services.imgui) {
    m_camera_view_projection = VLA::Matrix4x4f::Ortho(-1.0f, 1.0f, -1.0f, 1.0f, -1.0f, 1.0f);
  }
  ~View();

  void on_update(float dt) { m_layers.update(dt); }
  void on_gui_render();

  void on_event(EventVariant& e) { m_layers.on_event(e); }
  void on_render(vk::CommandBuffer cmd);
  void on_fixed_update(double dt) { m_layers.on_fixed_update(dt); }

  // --- Layer management ---
  template<typename T, typename... Args>
    requires std::is_base_of_v<Layer, T>
  T& push_layer(Args&&... args) {
    auto& layer   = m_layers.push_layer<T>(std::forward<Args>(args)...);
    layer.view_id = m_view_id;
    layer.view    = this;
    layer.app     = m_app;
    return layer;
  }

  template<typename T, typename... Args>
    requires std::is_base_of_v<Layer, T>
  T& push_layer(int stageOrder, Args&&... args) {
    auto& layer    = m_layers.push_layer<T>(std::forward<Args>(args)...);
    layer.stage_id = stageOrder;
    layer.view_id  = m_view_id;
    layer.view     = this;
    layer.app      = m_app;
    return layer;
  }

  // --- Identity ---
  const std::string&   get_name() const { return m_name; }
  [[nodiscard]] ViewId get_view_id() const { return m_view_id; }
  [[nodiscard]] bool   is_open() const { return m_open; }
  void                 set_open(bool open) { m_open = open; }

  // --- Layers ---
  LayerList&       get_layers() { return m_layers; }
  const LayerList& get_layers() const { return m_layers; }

  // --- Viewport state ---
  vk::Extent2D get_extent() const { return m_extent; }
  void         resize(vk::Extent2D extent);

  AspectMode get_aspect_mode() const { return m_aspect_mode; }
  void       set_aspect_mode(AspectMode mode) {
    m_aspect_mode = mode;
    resize(m_extent);
  }

  RenderMode get_render_mode() const { return m_render_mode; }
  void       set_render_mode(RenderMode mode) { m_render_mode = mode; }

  const VLA::Matrix4x4f& get_projection() const { return m_camera_view_projection; }

  /// Returns true (once) if the extent changed since the last call.
  bool consume_extent_changed() {
    bool changed     = m_extent_changed;
    m_extent_changed = false;
    return changed;
  }

  // --- Diagnostics (ImGui region tracking) ---
  ImVec2 get_content_region_pos() const { return m_content_region_pos; }
  ImVec2 get_content_region_extent() const { return m_content_region_extent; }

  // --- Viewport lifecycle ---
  vk::Extent2D    get_im_gui_extent();
  vk::Format      get_color_format() const { return m_color_format; }
  vk::Format      get_depth_format() const { return m_depth_format; }
  vk::ImageView   get_color_view() const { return *m_color_view; }
  vk::ImageView   get_depth_view() const { return *m_depth_view; }
  VkDescriptorSet get_display_texture() const { return m_display_tex_ds; }

  void create_viewport(VkExtent2D initialExtent = {1280, 720});
  void destroy_viewport();


  vk::Format find_depth_format();
  void       create_vulkan_resources();

  std::string m_name;
  LayerList   m_layers;
  bool        m_open = true;
  ViewId      m_view_id;

  VulkanContext&  m_vulkan_ctx;
  SDImGuiContext& m_imgui_ctx;
  Application*    m_app = nullptr;
  // ImGui content region (updated each frame in OnGuiRender)
  ImVec2 m_content_region_pos{0, 0};
  ImVec2 m_content_region_extent{0, 0};

  // Vulkan rendering resources
  vk::Image           m_color_image      = VK_NULL_HANDLE;
  VmaAllocation       m_color_allocation = VK_NULL_HANDLE;
  vk::UniqueImageView m_color_view;

  vk::Image           m_depth_image      = VK_NULL_HANDLE;
  VmaAllocation       m_depth_allocation = VK_NULL_HANDLE;
  vk::UniqueImageView m_depth_view;

  VkDescriptorSet m_display_tex_ds = VK_NULL_HANDLE; // imgui::image

  vk::Format      m_color_format = vk::Format::eR8G8B8A8Unorm;
  vk::Format      m_depth_format = vk::Format::eUndefined;
  VLA::Matrix4x4f m_camera_view_projection;
  vk::Extent2D    m_extent         = {1280, 720};
  bool            m_extent_changed = false;

  AspectMode m_aspect_mode = AspectMode::BEST_FIT;
  RenderMode m_render_mode = RenderMode::SHADED;

  friend class Application; // needs mViewId assignment
  friend class ViewManager;
};


} // namespace sd

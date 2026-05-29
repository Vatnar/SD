#pragma once

#include <deque>

#include "Application.hpp"
#include "Core/Layer.hpp"

namespace sd {

class Scene;
class View;
class Event;

class EngineDebugLayer : public Panel {
public:
  explicit EngineDebugLayer(Scene* scene = nullptr) : Panel("EngineDebug", scene) {}

  void on_update(float dt) override;
  void on_fixed_update(double dt) override;
  void on_event(Event& e) override;
  void on_im_gui_menu_bar() override;
  void on_gui_render() override;

private:
  void display_view_info(View* selected_view);
  void display_scene_selector();
  void display_ecs_inspector();
  void display_event_log();
  void display_layout_menu();
  void display_save_layout_dialog();
  void display_delete_layout_dialog();
  void display_overwrite_confirmation_dialog();
  void handle_layout_shortcuts();
  void handle_debug_shortcuts();

  // Dialog state
  bool m_show_save_layout_dialog = false;
  bool m_show_delete_layout_dialog = false;
  bool m_show_overwrite_confirmation = false;
  bool m_save_dialog_initialized = false;  // Track if buffer has been cleared
  std::string m_pending_layout_name;
  std::array<char, 64> m_layout_name_buffer{};  // For save dialog input (replaces static)

  // Window visibility - LayoutManager can control these
  bool m_show_view_inspector = true;
  bool m_show_scene_inspector = true;
  bool m_show_event_log = true;
  bool m_show_renderer_info = true;
  bool m_show_context_overlay = false;
  
public:
  // LayoutManager interface
  void set_view_inspector_visible(bool visible) { m_show_view_inspector = visible; }
  void set_scene_inspector_visible(bool visible) { m_show_scene_inspector = visible; }
  void set_event_log_visible(bool visible) { m_show_event_log = visible; }
  void set_renderer_info_visible(bool visible) { m_show_renderer_info = visible; }
  void set_context_overlay_visible(bool visible) { m_show_context_overlay = visible; }
  
  bool is_view_inspector_visible() const { return m_show_view_inspector; }
  bool is_scene_inspector_visible() const { return m_show_scene_inspector; }
  bool is_event_log_visible() const { return m_show_event_log; }
  bool is_renderer_info_visible() const { return m_show_renderer_info; }
  bool is_context_overlay_visible() const { return m_show_context_overlay; }
  
  // Apply a preset configuration (for LayoutManager)
  void apply_preset_configuration(bool inspectors_visible, bool log_visible, bool renderer_visible);

  bool m_log_events = false;
  bool m_log_scene_changes = false;
  bool m_log_view_resizes = false;
  bool m_log_entity_lifecycle = false;

  // Log viewer filters
  char m_log_search_buffer[128]{};
  int m_log_level_filter = 0;  // 0 = All, 1 = Debug+, 2 = Info+, 3 = Warn+, 4 = Error+
  bool m_log_filter_initialized = false;

  // Category tree for log filtering
  struct CategoryNode {
    std::string name;        // display name (last segment)
    std::string full_path;    // e.g., "Engine/Renderer"
    bool visible = true;
    std::vector<CategoryNode> children;
  };
  CategoryNode m_category_root{"", "", true, {}};
  bool m_category_tree_built = false;

  void build_category_tree();
  void render_category_node(CategoryNode& node);
  bool is_log_visible(const std::string& category);
  void set_category_visible(CategoryNode& node, bool visible);

  float m_timer = 0.0f;
  int m_update_count = 0;
  int m_fixed_update_count = 0;
  int m_prev_fixed = 0;
  int m_prev_update = 0;

  Scene* m_selected_scene = nullptr;
  std::optional<ViewId> m_selected_view_id;  // Store ID instead of pointer to avoid dangling

  // Entity lifecycle tracking (for m_log_entity_lifecycle)
  Scene* m_prev_scene_for_entity_count = nullptr;
  int m_prev_entity_count = -1;
  
  // Debug shortcut state (was static in HandleDebugShortcuts)
  bool m_debug_mode_active = false;
  float m_debug_mode_timer = 0.0f;
};

} // namespace SD

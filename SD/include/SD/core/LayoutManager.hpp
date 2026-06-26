#pragma once

#include <map>
#include <string>
#include <vector>

#include "SD/export.hpp"

namespace sd {

struct ApplicationRuntime;

/// Manages window layouts using ImGui DockBuilder for presets and INI for user layouts
struct SD_EXPORT LayoutManager {
  enum class Preset {
    DEFAULT,
    MINIMAL
  };

  LayoutManager();
  ~LayoutManager() = default;

  /// Initialize - loads last used layout name but doesn't apply yet
  void init();

  /// Apply a preset layout (uses DockBuilder API) - call this after dockspace exists
  void apply_preset(Preset preset, ApplicationRuntime runtime);

  /// Apply the pending layout (call from first frame when dockspace is ready)
  void apply_pending_layout(ApplicationRuntime runtime);

  /// Save current layout to a user-named file
  void save_layout(const std::string& name);

  /// Load a user-saved layout
  bool load_layout(const std::string& name, ApplicationRuntime runtime);

  /// Delete a user layout
  void delete_layout(const std::string& name, ApplicationRuntime runtime);

  /// Get list of user-saved layout names
  std::vector<std::string> get_user_layout_names() const;

  /// Check if a user layout exists
  bool has_layout(const std::string& name) const;

  /// Get the currently active layout/preset name
  const std::string& get_current_layout() const { return m_current_layout; }


  void        ensure_layouts_directory_exists();
  std::string get_layout_path(const std::string& name) const;
  void        save_current_layout_name();
  void        refresh_layout_list();

  std::map<std::string, std::string> m_user_layouts;
  std::string                        m_current_layout = "Default";
  std::string                        m_pending_layout;
  bool                               m_is_initialized             = false;
  bool                               m_has_applied_initial_layout = false;
};

} // namespace sd

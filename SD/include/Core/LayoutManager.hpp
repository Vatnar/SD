#pragma once

#include <string>
#include <vector>
#include <map>

#include "Core/Base.hpp"

namespace SD {

/// Manages window layouts using ImGui DockBuilder for presets and INI for user layouts
class LayoutManager {
public:
  enum class Preset {
    Default,
    Minimal
  };

  LayoutManager();
  ~LayoutManager() = default;

  /// Initialize - loads last used layout name but doesn't apply yet
  void Init();
  
  /// Apply a preset layout (uses DockBuilder API) - call this after dockspace exists
  void ApplyPreset(Preset preset);
  
  /// Apply the pending layout (call from first frame when dockspace is ready)
  void ApplyPendingLayout();
  
  /// Save current layout to a user-named file
  void SaveLayout(const std::string& name);
  
  /// Load a user-saved layout
  bool LoadLayout(const std::string& name);
  
  /// Delete a user layout
  void DeleteLayout(const std::string& name);
  
  /// Get list of user-saved layout names
  std::vector<std::string> GetUserLayoutNames() const;
  
  /// Check if a user layout exists
  bool HasLayout(const std::string& name) const;

  /// Get the currently active layout/preset name
  const std::string& GetCurrentLayout() const { return mCurrentLayout; }

private:
  void EnsureLayoutsDirectoryExists();
  std::string GetLayoutPath(const std::string& name) const;
  void SaveCurrentLayoutName();
  void RefreshLayoutList();
  
  std::map<std::string, std::string> mUserLayouts; // name -> filepath
  std::string mCurrentLayout = "Default";
  std::string mPendingLayout; // Layout to apply once dockspace exists
  bool mInitialized = false;
  bool mHasAppliedInitialLayout = false;
};

} // namespace SD

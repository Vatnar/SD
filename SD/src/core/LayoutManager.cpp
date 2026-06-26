#include "SD/core/LayoutManager.hpp"

#include <filesystem>
#include <fstream>
#include <imgui.h>
#include <imgui_internal.h>

#include "SD/core/ApplicationRuntime.hpp"
#include "SD/core/Layer.hpp"
#include "SD/core/SceneManager.hpp"
#include "SD/core/ViewManager.hpp"
#include "SD/core/layers/EngineDebugLayer.hpp"
#include "SD/core/logging.hpp"

namespace sd {

LayoutManager::LayoutManager() = default;

void LayoutManager::init() {
  if (m_is_initialized)
    return;

  ensure_layouts_directory_exists();
  refresh_layout_list();

  // Load last used layout name if it exists
  std::ifstream last_layout_file("layouts/.current");
  if (last_layout_file.is_open()) {
    std::getline(last_layout_file, m_current_layout);
    last_layout_file.close();
  }

  // Don't apply layout yet - dockspace doesn't exist
  // Store what layout to apply later
  if (m_current_layout == "Minimal") {
    m_pending_layout = "Minimal";
  } else if (m_current_layout == "Default") {
    m_pending_layout = "Default";
  } else if (!m_current_layout.empty() && has_layout(m_current_layout)) {
    m_pending_layout = m_current_layout;
  } else {
    m_pending_layout = "Default";
  }

  m_is_initialized = true;
}

void LayoutManager::apply_preset(Preset preset, ApplicationRuntime runtime) {
  if (!ImGui::GetCurrentContext()) {
    log::engine::warn("Cannot apply preset: ImGui context not available");
    return;
  }

  ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");

  // Remove existing layout and create fresh dockspace
  ImGui::DockBuilderRemoveNode(dockspace_id);
  ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
  ImGui::DockBuilderSetNodeSize(dockspace_id, ImGui::GetMainViewport()->Size);

  // RAII guard to ensure DockBuilderFinish is always called
  struct DockBuilderGuard {
    ImGuiID id;
    explicit DockBuilderGuard(ImGuiID id) : id(id) {}
    ~DockBuilderGuard() { ImGui::DockBuilderFinish(id); }
  };
  DockBuilderGuard guard(dockspace_id);

  // Find EngineDebugLayer to control inspector windows
  EngineDebugLayer* debug_layer = runtime.global_layers.Get<EngineDebugLayer>();

  if (preset == Preset::MINIMAL) {
    // Close all views except Game
    runtime.views.for_each([&](View& view) {
      if (view.get_name() != "Game") {
        view.set_open(false);
      }
    });

    // Open Game view
    auto game_view = runtime.views.get("Game");
    if (game_view) {
      game_view->get().set_open(true);
      ImGui::DockBuilderDockWindow("Game", dockspace_id);
    }

    // Hide all inspector windows
    if (debug_layer) {
      debug_layer->apply_preset_configuration(false, false, false);
    }

    m_current_layout = "Minimal";
    log::engine::info("Applied Minimal preset");

  } else { // Default
    // Ensure all views are open
    runtime.views.for_each([](View& view) { view.set_open(true); });

    // Show all inspector windows
    if (debug_layer) {
      debug_layer->apply_preset_configuration(true, true, true);
    }

    // Split: Left side = Game (70%), Right side = inspectors (30%)
    ImGuiID dock_id_left, dock_id_right;
    ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Left, 0.70f, &dock_id_left, &dock_id_right);

    // Right side: split vertically for inspector panels
    ImGuiID dock_id_right_top, dock_id_right_bottom;
    ImGui::DockBuilderSplitNode(dock_id_right,
                                ImGuiDir_Up,
                                0.50f,
                                &dock_id_right_top,
                                &dock_id_right_bottom);

    // Dock windows
    auto game_view = runtime.views.get("Game");
    if (game_view) {
      ImGui::DockBuilderDockWindow("Game", dock_id_left);
    }

    // Right top: View Inspector and Scene Inspector (tabbed)
    ImGui::DockBuilderDockWindow("View Inspector", dock_id_right_top);
    ImGui::DockBuilderDockWindow("Scene Inspector", dock_id_right_top);

    // Right bottom: Renderer Info and Engine Log (tabbed)
    ImGui::DockBuilderDockWindow("Renderer Info", dock_id_right_bottom);
    ImGui::DockBuilderDockWindow("Engine Log", dock_id_right_bottom);

    m_current_layout = "Default";
    log::engine::info("Applied Default preset");
  }

  save_current_layout_name();
  // DockBuilderFinish called automatically by guard
}

void LayoutManager::apply_pending_layout(ApplicationRuntime runtime) {
  if (m_has_applied_initial_layout || m_pending_layout.empty())
    return;

  // Check if dockspace exists
  ImGuiID        dockspace_id = ImGui::GetID("MyDockSpace");
  ImGuiDockNode* node         = ImGui::DockBuilderGetNode(dockspace_id);
  if (!node)
    return; // Dockspace not ready yet

  // Apply the pending layout
  if (m_pending_layout == "Minimal") {
    apply_preset(Preset::MINIMAL, runtime);
  } else if (m_pending_layout == "Default") {
    apply_preset(Preset::DEFAULT, runtime);
  } else if (has_layout(m_pending_layout)) {
    load_layout(m_pending_layout, runtime);
  }

  m_pending_layout.clear();
  m_has_applied_initial_layout = true;
}

void LayoutManager::save_layout(const std::string& name) {
  ensure_layouts_directory_exists();
  std::string path = get_layout_path(name);

  // Temporarily change ImGui's INI filename to save to our layout file
  ImGuiIO&    io               = ImGui::GetIO();
  const char* old_ini_filename = io.IniFilename;
  io.IniFilename               = path.c_str();

  // Save ImGui settings
  ImGui::SaveIniSettingsToDisk(path.c_str());

  // Restore original INI filename
  io.IniFilename = old_ini_filename;

  m_current_layout = name;
  save_current_layout_name();
  refresh_layout_list();

  log::engine::info("Layout '{}' saved to {}", name, path);
}

bool LayoutManager::load_layout(const std::string& name, ApplicationRuntime runtime) {
  if (!has_layout(name)) {
    log::engine::warn("Layout '{}' not found", name);
    return false;
  }

  std::string path = m_user_layouts[name];

  // Temporarily change ImGui's INI filename
  ImGuiIO&    io               = ImGui::GetIO();
  const char* old_ini_filename = io.IniFilename;
  io.IniFilename               = path.c_str();

  // Load ImGui settings
  ImGui::LoadIniSettingsFromDisk(path.c_str());

  // Restore original INI filename
  io.IniFilename = old_ini_filename;

  // Update view visibility based on what ImGui knows
  ImGuiContext* ctx = ImGui::GetCurrentContext();
  if (ctx) {
    runtime.views.for_each([&](View& view) {
      ImGuiWindow* imgui_window = ImGui::FindWindowByName(view.get_name().c_str());
      if (imgui_window) {
        view.set_open(true);
      }
    });
  }

  m_current_layout = name;
  save_current_layout_name();

  log::engine::info("Layout '{}' loaded from {}", name, path);
  return true;
}

void LayoutManager::delete_layout(const std::string& name, ApplicationRuntime runtime) {
  if (!has_layout(name))
    return;

  std::string     path = m_user_layouts[name];
  std::error_code ec;
  std::filesystem::remove(path, ec);

  if (ec) {
    log::engine::warn("Failed to delete layout file '{}': {}", path, ec.message());
    return;
  }

  if (m_current_layout == name) {
    m_current_layout = "Default";
    apply_preset(Preset::DEFAULT, runtime);
  }

  refresh_layout_list();
  log::engine::info("Layout '{}' deleted", name);
}

std::vector<std::string> LayoutManager::get_user_layout_names() const {
  std::vector<std::string> names;
  for (const auto& name : m_user_layouts | std::views::keys) {
    names.push_back(name);
  }
  return names;
}

bool LayoutManager::has_layout(const std::string& name) const {
  return m_user_layouts.find(name) != m_user_layouts.end();
}

void LayoutManager::ensure_layouts_directory_exists() {
  std::filesystem::create_directories("layouts");
}

std::string LayoutManager::get_layout_path(const std::string& name) const {
  return "layouts/" + name + ".ini";
}

void LayoutManager::save_current_layout_name() {
  ensure_layouts_directory_exists();
  std::ofstream last_layout_file("layouts/.current");
  if (last_layout_file.is_open()) {
    if (!(last_layout_file << m_current_layout)) {
      log::engine::warn("Failed to write current layout name to file");
    }
    last_layout_file.close();
  } else {
    log::engine::warn("Failed to open .current layout file for writing");
  }
}

void LayoutManager::refresh_layout_list() {
  m_user_layouts.clear();

  std::error_code ec;
  if (!std::filesystem::exists("layouts"))
    return;

  for (const auto& entry : std::filesystem::directory_iterator("layouts", ec)) {
    if (!entry.is_regular_file())
      continue;

    std::string filename = entry.path().filename().string();
    if (filename.ends_with(".ini") && filename != ".current") {
      // Extract layout name from "<name>.ini"
      std::string name = filename.substr(0, filename.size() - 4);
      if (!name.empty()) {
        m_user_layouts[name] = entry.path().string();
      }
    }
  }
}

} // namespace sd

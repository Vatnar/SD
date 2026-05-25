#include "Core/LayoutManager.hpp"

#include <imgui.h>
#include <imgui_internal.h>  // For DockBuilder API

#include <filesystem>
#include <fstream>

#include "Application.hpp"
#include "Core/Layer.hpp"
#include "Core/Layers/EngineDebugLayer.hpp"
#include "Core/Logging.hpp"
#include "Core/View.hpp"

namespace SD {

LayoutManager::LayoutManager() = default;

void LayoutManager::Init() {
  if (mInitialized) return;

  EnsureLayoutsDirectoryExists();
  RefreshLayoutList();

  // Load last used layout name if it exists
  std::ifstream lastLayoutFile("layouts/.current");
  if (lastLayoutFile.is_open()) {
    std::getline(lastLayoutFile, mCurrentLayout);
    lastLayoutFile.close();
  }

  // Don't apply layout yet - dockspace doesn't exist
  // Store what layout to apply later
  if (mCurrentLayout == "Minimal") {
    mPendingLayout = "Minimal";
  } else if (mCurrentLayout == "Default") {
    mPendingLayout = "Default";
  } else if (!mCurrentLayout.empty() && HasLayout(mCurrentLayout)) {
    mPendingLayout = mCurrentLayout;
  } else {
    mPendingLayout = "Default";
  }

  mInitialized = true;
}

void LayoutManager::ApplyPreset(Preset preset) {
  if (!ImGui::GetCurrentContext()) {
    SD::Log::Engine::Warn("Cannot apply preset: ImGui context not available");
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
  
  auto& app = Application::Get();
  
  // Find EngineDebugLayer to control inspector windows
  EngineDebugLayer* debugLayer = nullptr;
  for (auto& layer : app.GetGlobalLayers()) {
    if (auto* edl = dynamic_cast<EngineDebugLayer*>(layer.get())) {
      debugLayer = edl;
      break;
    }
  }

  if (preset == Preset::Minimal) {
    // Close all views except Game
    for (const auto& [id, view] : app.GetViews()) {
      if (view->GetName() != "Game") {
        view->SetOpen(false);
      }
    }

    // Open Game view
    auto gameView = app.GetView("Game");
    if (gameView) {
      gameView->get().SetOpen(true);
      ImGui::DockBuilderDockWindow("Game", dockspace_id);
    }

    // Hide all inspector windows
    if (debugLayer) {
      debugLayer->ApplyPresetConfiguration(false, false, false);
    }

    mCurrentLayout = "Minimal";
    SD::Log::Engine::Info("Applied Minimal preset");

  } else { // Default
    // Ensure all views are open
    for (const auto& [id, view] : app.GetViews()) {
      view->SetOpen(true);
    }

    // Show all inspector windows
    if (debugLayer) {
      debugLayer->ApplyPresetConfiguration(true, true, true);
    }

    // Split: Left side = Game (70%), Right side = inspectors (30%)
    ImGuiID dock_id_left, dock_id_right;
    ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Left, 0.70f, &dock_id_left, &dock_id_right);

    // Right side: split vertically for inspector panels
    ImGuiID dock_id_right_top, dock_id_right_bottom;
    ImGui::DockBuilderSplitNode(dock_id_right, ImGuiDir_Up, 0.50f, &dock_id_right_top, &dock_id_right_bottom);

    // Dock windows
    auto gameView = app.GetView("Game");
    if (gameView) {
      ImGui::DockBuilderDockWindow("Game", dock_id_left);
    }

    // Right top: View Inspector and Scene Inspector (tabbed)
    ImGui::DockBuilderDockWindow("View Inspector", dock_id_right_top);
    ImGui::DockBuilderDockWindow("Scene Inspector", dock_id_right_top);

    // Right bottom: Renderer Info and Engine Log (tabbed)
    ImGui::DockBuilderDockWindow("Renderer Info", dock_id_right_bottom);
    ImGui::DockBuilderDockWindow("Engine Log", dock_id_right_bottom);

    mCurrentLayout = "Default";
    SD::Log::Engine::Info("Applied Default preset");
  }
  
  SaveCurrentLayoutName();
  // DockBuilderFinish called automatically by guard
}

void LayoutManager::ApplyPendingLayout() {
  if (mHasAppliedInitialLayout || mPendingLayout.empty()) return;
  
  // Check if dockspace exists
  ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
  ImGuiDockNode* node = ImGui::DockBuilderGetNode(dockspace_id);
  if (!node) return; // Dockspace not ready yet
  
  // Apply the pending layout
  if (mPendingLayout == "Minimal") {
    ApplyPreset(Preset::Minimal);
  } else if (mPendingLayout == "Default") {
    ApplyPreset(Preset::Default);
  } else if (HasLayout(mPendingLayout)) {
    LoadLayout(mPendingLayout);
  }
  
  mPendingLayout.clear();
  mHasAppliedInitialLayout = true;
}

void LayoutManager::SaveLayout(const std::string& name) {
  EnsureLayoutsDirectoryExists();
  std::string path = GetLayoutPath(name);
  
  // Temporarily change ImGui's INI filename to save to our layout file
  ImGuiIO& io = ImGui::GetIO();
  const char* oldIniFilename = io.IniFilename;
  io.IniFilename = path.c_str();
  
  // Save ImGui settings
  ImGui::SaveIniSettingsToDisk(path.c_str());
  
  // Restore original INI filename
  io.IniFilename = oldIniFilename;
  
  mCurrentLayout = name;
  SaveCurrentLayoutName();
  RefreshLayoutList();
  
  SD::Log::Engine::Info("Layout '{}' saved to {}", name, path);
}

bool LayoutManager::LoadLayout(const std::string& name) {
  if (!HasLayout(name)) {
    SD::Log::Engine::Warn("Layout '{}' not found", name);
    return false;
  }
  
  std::string path = mUserLayouts[name];
  
  // Temporarily change ImGui's INI filename
  ImGuiIO& io = ImGui::GetIO();
  const char* oldIniFilename = io.IniFilename;
  io.IniFilename = path.c_str();
  
  // Load ImGui settings
  ImGui::LoadIniSettingsFromDisk(path.c_str());
  
  // Restore original INI filename
  io.IniFilename = oldIniFilename;
  
  // Update view visibility based on what ImGui knows
  auto& app = Application::Get();
  ImGuiContext* ctx = ImGui::GetCurrentContext();
  if (ctx) {
    for (const auto& [id, view] : app.GetViews()) {
      // Check if ImGui has this window
      ImGuiWindow* imguiWindow = ImGui::FindWindowByName(view->GetName().c_str());
      if (imguiWindow) {
        // Sync ImGui's collapsed state to View's open state
        // Note: ImGui doesn't track "closed" vs "collapsed" the same way
        // For now, we'll just ensure the view is open
        view->SetOpen(true);
      }
    }
  }
  
  mCurrentLayout = name;
  SaveCurrentLayoutName();
  
  SD::Log::Engine::Info("Layout '{}' loaded from {}", name, path);
  return true;
}

void LayoutManager::DeleteLayout(const std::string& name) {
  if (!HasLayout(name)) return;
  
  std::string path = mUserLayouts[name];
  std::error_code ec;
  std::filesystem::remove(path, ec);
  
  if (ec) {
    SD::Log::Engine::Warn("Failed to delete layout file '{}': {}", path, ec.message());
    return;
  }
  
  if (mCurrentLayout == name) {
    mCurrentLayout = "Default";
    ApplyPreset(Preset::Default);
  }
  
  RefreshLayoutList();
  SD::Log::Engine::Info("Layout '{}' deleted", name);
}

std::vector<std::string> LayoutManager::GetUserLayoutNames() const {
  std::vector<std::string> names;
  for (const auto& [name, _] : mUserLayouts) {
    names.push_back(name);
  }
  return names;
}

bool LayoutManager::HasLayout(const std::string& name) const {
  return mUserLayouts.find(name) != mUserLayouts.end();
}

void LayoutManager::EnsureLayoutsDirectoryExists() {
  std::filesystem::create_directories("layouts");
}

std::string LayoutManager::GetLayoutPath(const std::string& name) const {
  return "layouts/" + name + ".ini";
}

void LayoutManager::SaveCurrentLayoutName() {
  EnsureLayoutsDirectoryExists();
  std::ofstream lastLayoutFile("layouts/.current");
  if (lastLayoutFile.is_open()) {
    if (!(lastLayoutFile << mCurrentLayout)) {
      SD::Log::Engine::Warn("Failed to write current layout name to file");
    }
    lastLayoutFile.close();
  } else {
    SD::Log::Engine::Warn("Failed to open .current layout file for writing");
  }
}

void LayoutManager::RefreshLayoutList() {
  mUserLayouts.clear();
  
  std::error_code ec;
  if (!std::filesystem::exists("layouts")) return;
  
  for (const auto& entry : std::filesystem::directory_iterator("layouts", ec)) {
    if (!entry.is_regular_file()) continue;
    
    std::string filename = entry.path().filename().string();
    if (filename.ends_with(".ini") && filename != ".current") {
      // Extract layout name from "<name>.ini"
      std::string name = filename.substr(0, filename.size() - 4);
      if (!name.empty()) {
        mUserLayouts[name] = entry.path().string();
      }
    }
  }
}

} // namespace SD

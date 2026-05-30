#include <chrono>
#include <dlfcn.h>
#include <filesystem>
#include <iostream>
#include <system_error>
#include <thread>

#include "Application.hpp"
#include "ConfigLoader.hpp"
#include "game.h"

static void* g_game_handle = nullptr;
static GameAPI g_game_api = {};
static GameState g_game_state = {};
static std::filesystem::file_time_type g_last_so_write_time;
static int g_reload_count = 0;

static std::string get_live_so_name(const std::string& game_so) {
  g_reload_count++;
  return "./" + game_so + "_live_" + std::to_string(g_reload_count);
}

// Wait for file to stop changing (build system finished writing)
static bool wait_for_file_stable(const std::filesystem::path& path,
                              std::chrono::milliseconds timeout = std::chrono::seconds(2),
                              std::chrono::milliseconds check_interval = std::chrono::milliseconds(50)) {
  std::error_code ec;
  auto last_size = std::filesystem::file_size(path, ec);
  if (ec) {
    sd::log::engine::error("File stability check failed (file_size): {}", ec.message());
    return false;
  }
  auto last_time = std::filesystem::last_write_time(path, ec);
  if (ec) {
    sd::log::engine::error("File stability check failed (last_write_time): {}", ec.message());
    return false;
  }
  auto start = std::chrono::steady_clock::now();

  while (std::chrono::steady_clock::now() - start < timeout) {
    std::this_thread::sleep_for(check_interval);

    auto current_size = std::filesystem::file_size(path, ec);
    if (ec) {
      sd::log::engine::error("File stability check failed (file_size): {}", ec.message());
      return false;
    }
    auto current_time = std::filesystem::last_write_time(path, ec);
    if (ec) {
      sd::log::engine::error("File stability check failed (last_write_time): {}", ec.message());
      return false;
    }

    if (current_size == last_size && current_time == last_time) {
      return true;  // File stable
    }

    last_size = current_size;
    last_time = current_time;
  }

  sd::log::engine::warn("File {} still changing after {}ms, skipping reload", path.string(), timeout.count());
  return false;
}

// Load new game code while keeping old code mapped
// Returns true on success, false on failure (old code remains valid)
static bool load_game_code_safe(const std::string& game_so_path,
                             void** out_new_handle, GameAPI* out_new_api) {
  // Wait for file to be stable (build finished)
  if (!wait_for_file_stable(game_so_path)) {
    return false;
  }

  std::string game_so_filename = std::filesystem::path(game_so_path).filename().string();
  std::string live_so = get_live_so_name(game_so_filename);
  std::string temp_so = live_so + ".tmp";

  // Copy to temp file first (not the watched file)
  std::error_code ec;
  if (!std::filesystem::copy_file(game_so_path, temp_so,
                                  std::filesystem::copy_options::overwrite_existing, ec)) {
    sd::log::engine::error("Failed to copy game code: {}", ec.message());
    return false;
  }

  // Atomically rename temp to final - this ensures the loader only sees complete file
  std::filesystem::rename(temp_so, live_so, ec);
  if (ec) {
    sd::log::engine::error("Failed to rename game code: {}", ec.message());
    return false;
  }

  // Load new code BEFORE closing old code
  void* new_handle = dlopen(live_so.c_str(), RTLD_NOW);
  if (!new_handle) {
    const char* err = dlerror();
    sd::log::engine::error("dlopen failed: {}", err ? err : "unknown error");
    return false;
  }

  auto* get_api = reinterpret_cast<GetGameAPIFn>(dlsym(new_handle, GAME_GET_API_NAME));
  if (!get_api) {
    const char* err = dlerror();
    sd::log::engine::error("dlsym GetGameAPI failed: {}", err ? err : "unknown error");
    dlclose(new_handle);
    return false;
  }

  GameAPI new_api = get_api();
  if (!new_api.on_load) {
    sd::log::engine::error("Game code missing OnLoad function");
    dlclose(new_handle);
    return false;
  }

  std::printf("%p\t%p\t%p\n", new_api.on_update, new_api.on_load, new_api.on_unload);
  sd::log::engine::info("New game code loaded successfully");

  *out_new_handle = new_handle;
  *out_new_api = new_api;
  return true;
}

int main(int argc, char* argv[]) {
  sd::log::init();
  ConfigLoader config(argc, argv);
  const auto& cfg = config.get_config();

  sd::log::engine::info("HotReloadApp starting");
  sd::log::engine::info("  game-so-path: {}", cfg.game_so_path);
  sd::log::engine::info("  build-dir: {}", cfg.build_dir);
  sd::log::engine::info("  app-name: {}", cfg.app_name);
  sd::log::engine::info("  window: {}x{}", cfg.window_width, cfg.window_height);

  // Initial load (old style - no fallback possible)
  {
    std::string game_so_filename = std::filesystem::path(cfg.game_so_path).filename().string();
    std::string live_so = get_live_so_name(game_so_filename);
    std::string temp_so = live_so + ".tmp";

    std::error_code ec;
    if (!std::filesystem::copy_file(cfg.game_so_path, temp_so,
                                    std::filesystem::copy_options::overwrite_existing, ec)) {
      sd::engine_abort("Failed to copy game code: " + ec.message());
    }
    std::filesystem::rename(temp_so, live_so, ec);
    if (ec) {
      sd::engine_abort("Failed to rename game code: " + ec.message());
    }

    g_game_handle = dlopen(live_so.c_str(), RTLD_NOW);
    if (!g_game_handle) {
      sd::log::engine::error("Failed to load game code: {}", dlerror());
      return 1;
    }

    auto* get_api = reinterpret_cast<GetGameAPIFn>(dlsym(g_game_handle, GAME_GET_API_NAME));
    if (!get_api) {
      sd::log::engine::error("Failed to get game API: {}", dlerror());
      return 1;
    }

    g_game_api = get_api();
  }

  g_last_so_write_time = std::filesystem::last_write_time(cfg.game_so_path);

  sd::Application app({cfg.app_name.c_str(), cfg.window_width, cfg.window_height});

  // Initial load
  g_game_api.on_load(reinterpret_cast<SD_Application*>(&app), &g_game_state);

  // Run loop - check for .so changes
  while (app.is_running()) {
    // Check for reload (when user clicks Build in CLion)
    if (std::filesystem::exists(cfg.game_so_path)) {
      auto current_time = std::filesystem::last_write_time(cfg.game_so_path);
      if (current_time > g_last_so_write_time) {
        g_last_so_write_time = current_time;

        sd::log::engine::info("Detected change, reloading game code...");

        // Load NEW code first (while old code is still mapped)
        void* new_handle = nullptr;
        GameAPI new_api = {};
        if (!load_game_code_safe(cfg.game_so_path, &new_handle, &new_api)) {
          sd::log::engine::error("Failed to load new game code - keeping old code running");
          continue;  // Don't reload, keep using current code
        }

        // New code loaded successfully - now safe to unload old
        sd::log::engine::info("Unloading old game code...");

        // Unload old code
        if (g_game_api.on_unload) {
          g_game_api.on_unload(reinterpret_cast<SD_Application*>(&app), &g_game_state);
        }

        // CRITICAL: Wait for GPU to finish all work before destroying layers
        // This prevents use-after-free of Vulkan resources
        (void)app.get_vulkan_context().get_vulkan_device()->waitIdle();

        // Clear engine state before loading new code
        app.clear_game_layers();

        // Close old handle after layers are destroyed
        if (g_game_handle) {
          dlclose(g_game_handle);
        }

        // Switch to new code
        g_game_handle = new_handle;
        g_game_api = new_api;

        // Load new code with existing state
        g_game_api.on_load(reinterpret_cast<SD_Application*>(&app), &g_game_state);

        sd::log::engine::info("Game code reload completed successfully");
      }
    }

    // Frame
    app.frame();
  }

  // Cleanup
  if (g_game_api.on_unload) {
    g_game_api.on_unload(reinterpret_cast<SD_Application*>(&app), &g_game_state);
  }

  if (g_game_handle) {
    dlclose(g_game_handle);
  }

  return 0;
}

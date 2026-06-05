#include <chrono>
#include <dlfcn.h>
#include <filesystem>
#include <iostream>
#include <system_error>
#include <thread>

#include "ConfigLoader.hpp"
#include "SD/Application.hpp"
#include "SD/core/logging.hpp"
#include "SD/game_api.hpp"

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

static void*                           g_game_handle = nullptr;
static GameAPI                         g_game_api    = {};
static GameState                       g_game_state  = {};
static std::filesystem::file_time_type g_last_so_write_time;
static int                             g_reload_count = 0;

static std::filesystem::path get_live_so_path(const std::filesystem::path& source_so,
                                              int                          reload_index) {
  auto dir  = source_so.parent_path();
  auto stem = source_so.filename().string();
  return dir / (stem + "_live_" + std::to_string(reload_index));
}

static void cleanup_stale_live_copies(const std::filesystem::path& source_so) {
  auto            dir    = source_so.parent_path();
  auto            stem   = source_so.filename().string();
  std::string     prefix = stem + "_live_";
  std::error_code ec;
  for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
    if (ec)
      break;
    if (!entry.is_regular_file(ec))
      continue;
    if (ec)
      continue;
    auto name = entry.path().filename().string();
    if (name.starts_with(prefix)) {
      std::filesystem::remove(entry.path(), ec);
      if (!ec) {
        sd::log::engine::info("Removed stale live copy: {}", entry.path().string());
      }
    }
  }
}

static bool
wait_for_file_stable(const std::filesystem::path& path,
                     std::chrono::milliseconds    timeout        = std::chrono::seconds(2),
                     std::chrono::milliseconds    check_interval = std::chrono::milliseconds(50)) {
  std::error_code ec;
  auto            last_size = std::filesystem::file_size(path, ec);
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
      return true;
    }

    last_size = current_size;
    last_time = current_time;
  }

  sd::log::engine::warn("File {} still changing after {}ms, skipping reload", path.string(),
                        timeout.count());
  return false;
}

static bool load_game_code_safe(const std::filesystem::path& source_so, void** out_new_handle,
                                GameAPI* out_new_api) {
  if (!wait_for_file_stable(source_so)) {
    return false;
  }

  g_reload_count++;
  auto live_so = get_live_so_path(source_so, g_reload_count);
  auto temp_so = std::filesystem::path(live_so.string() + ".tmp");

  std::error_code ec;
  if (!std::filesystem::copy_file(source_so, temp_so,
                                  std::filesystem::copy_options::overwrite_existing, ec)) {
    sd::log::engine::error("Failed to copy game code: {}", ec.message());
    return false;
  }

  std::filesystem::rename(temp_so, live_so, ec);
  if (ec) {
    sd::log::engine::error("Failed to rename game code: {}", ec.message());
    return false;
  }

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

  if (new_api.api_version != GAME_API_VERSION) {
    sd::log::engine::error("Game code API version mismatch: expected {}, got {} — "
                           "the plugin was likely compiled against a different game.h",
                           GAME_API_VERSION, new_api.api_version);
    dlclose(new_handle);
    return false;
  }

  if (new_api.struct_size != sizeof(GameAPI)) {
    sd::log::engine::error("Game code struct size mismatch: expected {}, got {} — "
                           "ABI break: recompile both sides with the same game.h",
                           sizeof(GameAPI), new_api.struct_size);
    dlclose(new_handle);
    return false;
  }

  if (!new_api.on_load) {
    sd::log::engine::error("Game code missing OnLoad function");
    dlclose(new_handle);
    return false;
  }

  sd::log::engine::info("New game code loaded successfully (api_version={}, struct_size={})",
                        new_api.api_version, new_api.struct_size);

  *out_new_handle = new_handle;
  *out_new_api    = new_api;
  return true;
}

int main(int argc, char* argv[]) {
  sd::log::init();
  hr::ConfigLoader config(argc, argv);
  const auto&      cfg = config.get_config();

#ifdef SD_DEBUG
  sd::log::engine::warn("Running in debug mode");
#endif
  sd::log::engine::info("HotReloadApp starting");
  sd::log::engine::info("  game-so-path: {}", cfg.game_so_path);
  sd::log::engine::info("  build-dir: {}", cfg.build_dir);
  sd::log::engine::info("  app-name: {}", cfg.app_name);
  sd::log::engine::info("  window: {}x{}", cfg.window_width, cfg.window_height);

  std::filesystem::path source_so(cfg.game_so_path);
  cleanup_stale_live_copies(source_so);

  {
    g_reload_count++;
    auto live_so = get_live_so_path(source_so, g_reload_count);
    auto temp_so = std::filesystem::path(live_so.string() + ".tmp");

    std::error_code ec;
    if (!std::filesystem::copy_file(source_so, temp_so,
                                    std::filesystem::copy_options::overwrite_existing, ec)) {
      sd::log::engine::critical("Failed to copy game code: {}", ec.message());
    }
    std::filesystem::rename(temp_so, live_so, ec);
    if (ec) {
      sd::log::engine::critical("Failed to rename game code: {}", ec.message());
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

    if (g_game_api.api_version != GAME_API_VERSION) {
      sd::log::engine::error("Game code API version mismatch: expected {}, got {}",
                             GAME_API_VERSION, g_game_api.api_version);
      return 1;
    }
    if (g_game_api.struct_size != sizeof(GameAPI)) {
      sd::log::engine::error("Game code struct size mismatch: expected {}, got {}", sizeof(GameAPI),
                             g_game_api.struct_size);
      return 1;
    }
  }

  {
    std::error_code ec;
    g_last_so_write_time = std::filesystem::last_write_time(source_so, ec);
    if (ec) {
      sd::log::engine::error("Failed to get initial write time: {}", ec.message());
      return 1;
    }
  }

  sd::ApplicationSpecification app_spec{
      .name            = cfg.app_name,
      .width           = cfg.window_width,
      .height          = cfg.window_height,
      .enableHotReload = true,
      .gameSoPath      = cfg.game_so_path,
  };
  sd::Application app(app_spec);

  g_game_api.on_load(reinterpret_cast<SD_Application*>(&app), &g_game_state);

  while (app.is_running) {
    std::error_code ec;
    if (!std::filesystem::exists(source_so, ec) || ec) {
      continue;
    }

    auto current_time = std::filesystem::last_write_time(source_so, ec);
    if (ec) {
      sd::log::engine::warn("Failed to check write time: {}", ec.message());
      continue;
    }

    if (current_time > g_last_so_write_time) {
      g_last_so_write_time = current_time;

      sd::log::engine::info("Detected change, reloading game code...");

      void*   new_handle = nullptr;
      GameAPI new_api    = {};
      if (!load_game_code_safe(source_so, &new_handle, &new_api)) {
        sd::log::engine::error("Failed to load new game code - keeping old code running");
        continue;
      }

      sd::log::engine::info("Unloading old game code...");

      (void)app.services().vulkan.get_vulkan_device()->waitIdle();

      if (g_game_api.on_unload) {
        g_game_api.on_unload(reinterpret_cast<SD_Application*>(&app), &g_game_state);
      }

      app.clear_game_layers();

      // NOTE: dlclose() decrements the reference count; it does NOT
      // guarantee synchronous unmap. Outstanding function pointers,
      // callbacks, lambdas, thread entries, ImGui hooks, ECS system
      // pointers, or Vulkan debug callbacks still referencing old code
      // make the reload unsound. Audit every cross-boundary reference.
      if (g_game_handle) {
        dlclose(g_game_handle);
      }

      g_game_handle = new_handle;
      g_game_api    = new_api;

      g_game_api.on_load(reinterpret_cast<SD_Application*>(&app), &g_game_state);

      sd::log::engine::info("Game code reload completed successfully");
    }

    app.frame();
  }

  if (g_game_api.on_unload) {
    g_game_api.on_unload(reinterpret_cast<SD_Application*>(&app), &g_game_state);
  }

  if (g_game_handle) {
    dlclose(g_game_handle);
  }

  return 0;
}

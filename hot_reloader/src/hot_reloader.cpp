#include <chrono>
#include <dlfcn.h>
#include <filesystem>
#include <iostream>
#include <system_error>
#include <thread>

#include "ConfigLoader.hpp"
#include "SD/Application.hpp"
#include "SD/core/ecs/ComponentFactory.hpp"
#include "SD/core/logging.hpp"
#include "SD/game_api.hpp"

static void*                           g_game_handle{nullptr};
static GameAPI                         g_game_api{};
static game::State                     g_game_state{};
static std::filesystem::file_time_type g_last_so_write_time{};
static int                             g_reload_count{};

// Accumulated old .so handles that are deliberately NOT closed during
// hot-reload (dlclose is unsound — it can unmap code still referenced by
// function pointers, lambdas, callbacks, etc.). On shutdown we close them
// all at once when nothing can possibly be in flight.
static std::vector<void*> g_stale_handles{};

// Cooldown timer to avoid spamming dlopen attempts when the file
// changes faster than we can reload or the build is broken.
using time_point = std::chrono::steady_clock::time_point;
static time_point g_last_reload_check{};

static std::filesystem::path get_live_so_path(const std::filesystem::path& source_so,
                                              int                          reload_index) {
  std::filesystem::path dir{source_so.parent_path()};
  std::string           stem{source_so.filename().string()};
  return dir / (stem + "_live_" + std::to_string(reload_index));
}

static void cleanup_stale_live_copies(const std::filesystem::path& source_so) {
  std::filesystem::path dir{source_so.parent_path()};
  std::string           stem{source_so.filename().string()};
  std::string           prefix{stem + "_live_"};
  std::error_code       ec{};
  for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
    if (ec)
      break;
    if (!entry.is_regular_file(ec))
      continue;
    if (ec)
      continue;
    std::string name{entry.path().filename().string()};
    if (name.starts_with(prefix)) {
      std::filesystem::remove(entry.path(), ec);
      if (!ec) {
        sd::log::engine::info("Removed stale live copy: {}", entry.path().string());
      }
    }
  }
}

// No file-stability polling: the build system writes atomically (rename),
// and dlopen with RTLD_NOW already fails on a corrupt .so. A failed reload
// just retries on the next file change.
static bool load_game_code_safe(const std::filesystem::path& source_so,
                                void**                       out_new_handle,
                                GameAPI*                     out_new_api) {
  g_reload_count++;
  std::filesystem::path live_so{get_live_so_path(source_so, g_reload_count)};
  std::filesystem::path temp_so{std::filesystem::path(live_so.string() + ".tmp")};

  std::error_code ec{};
  if (!std::filesystem::copy_file(source_so,
                                  temp_so,
                                  std::filesystem::copy_options::overwrite_existing,
                                  ec)) {
    sd::log::engine::error("Failed to copy game code: {}", ec.message());
    return false;
  }

  std::filesystem::rename(temp_so, live_so, ec);
  if (ec) {
    sd::log::engine::error("Failed to rename game code: {}", ec.message());
    return false;
  }

  void* new_handle{dlopen(live_so.c_str(), RTLD_NOW)};
  if (!new_handle) {
    const char* err{dlerror()};
    sd::log::engine::error("dlopen failed: {}", err ? err : "unknown error");
    return false;
  }

  GameAPI (*get_api)(){reinterpret_cast<GetGameAPIFn>(dlsym(new_handle, GAME_GET_API_NAME))};
  if (!get_api) {
    const char* err{dlerror()};
    sd::log::engine::error("dlsym GetGameAPI failed: {}", err ? err : "unknown error");
    dlclose(new_handle);
    return false;
  }

  GameAPI new_api{get_api()};

  if (new_api.api_version != GAME_API_VERSION) {
    sd::log::engine::error("Game code API version mismatch: expected {}, got {} — "
                           "the plugin was likely compiled against a different game.h",
                           GAME_API_VERSION,
                           new_api.api_version);
    dlclose(new_handle);
    return false;
  }

  if (new_api.struct_size != sizeof(GameAPI)) {
    sd::log::engine::error("Game code struct size mismatch: expected {}, got {} — "
                           "ABI break: recompile both sides with the same game.h",
                           sizeof(GameAPI),
                           new_api.struct_size);
    dlclose(new_handle);
    return false;
  }

  if (!new_api.on_load) {
    sd::log::engine::error("Game code missing OnLoad function");
    dlclose(new_handle);
    return false;
  }

  if (!new_api.on_reload) {
    sd::log::engine::error("Game code missing OnReload function — "
                           "hot-reload would push no layers or views");
    dlclose(new_handle);
    return false;
  }

  sd::log::engine::info("New game code loaded successfully (api_version={}, struct_size={})",
                        new_api.api_version,
                        new_api.struct_size);

  *out_new_handle = new_handle;
  *out_new_api    = new_api;
  return true;
}

bool load_game_api(std::filesystem::path source_so) {
  g_reload_count++;
  std::filesystem::path live_so{get_live_so_path(source_so, g_reload_count)};
  std::filesystem::path temp_so{std::filesystem::path(live_so.string() + ".tmp")};

  std::error_code ec{};
  if (!std::filesystem::copy_file(source_so,
                                  temp_so,
                                  std::filesystem::copy_options::overwrite_existing,
                                  ec)) {
    sd::log::engine::critical("Failed to copy game code: {}", ec.message());
  }
  std::filesystem::rename(temp_so, live_so, ec);
  if (ec) {
    sd::log::engine::critical("Failed to rename game code: {}", ec.message());
  }

  g_game_handle = dlopen(live_so.c_str(), RTLD_NOW);
  if (!g_game_handle) {
    sd::log::engine::error("Failed to load game code: {}", dlerror());
    return false;
  }

  GameAPI (*get_api)(){reinterpret_cast<GetGameAPIFn>(dlsym(g_game_handle, GAME_GET_API_NAME))};
  if (!get_api) {
    sd::log::engine::error("Failed to get game API: {}", dlerror());
    return false;
  }

  g_game_api = get_api();

  if (g_game_api.api_version != GAME_API_VERSION) {
    sd::log::engine::error("Game code API version mismatch: expected {}, got {}",
                           GAME_API_VERSION,
                           g_game_api.api_version);
    return false;
  }
  if (g_game_api.struct_size != sizeof(GameAPI)) {
    sd::log::engine::error("Game code struct size mismatch: expected {}, got {}",
                           sizeof(GameAPI),
                           g_game_api.struct_size);
    return false;
  }
  if (!g_game_api.on_load) {
    sd::log::engine::critical("Game code missing OnLoad function");
    return false;
  }
  if (!g_game_api.on_reload) {
    sd::log::engine::critical("Game code missing OnReload function");
    return false;
  }
  return true;
}


int main(int argc, char* argv[]) {
  sd::log::init();
  hr::ConfigLoader           config(argc, argv);
  const hr::HotReloadConfig& cfg{config.get_config()};

#ifdef SD_DEBUG
  sd::log::engine::warn("Running in debug mode");
#endif
  sd::log::engine::info("HotReloadApp starting");
  sd::log::engine::info("  game-so-path: {}", cfg.game_so_path);
  sd::log::engine::info("  build-dir: {}", cfg.build_dir);
  sd::log::engine::info("  app-name: {}", cfg.app_name);
  sd::log::engine::info("  window: {}x{}", cfg.window_width, cfg.window_height);

  std::filesystem::path source_so{cfg.game_so_path};
  cleanup_stale_live_copies(source_so);

  if (!load_game_api(source_so)) {
    std::exit(1);
  }

  std::error_code ec{};
  g_last_so_write_time = std::filesystem::last_write_time(source_so, ec);
  if (ec) {
    sd::log::engine::error("Failed to get initial write time: {}", ec.message());
    return 1;
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
    // ── Full restart request (from EngineDebugLayer "Restart Game") ────
    if (app.consume_restart_request()) {
      sd::log::engine::info("Restart requested, performing full game restart...");

      void*   new_handle{nullptr};
      GameAPI new_api{};
      if (!load_game_code_safe(source_so, &new_handle, &new_api)) {
        sd::log::engine::error("Failed to reload game code for restart");
        continue;
      }

      (void)app.services().vulkan.get_vulkan_device()->waitIdle();

      if (g_game_api.on_unload) {
        g_game_api.on_unload(reinterpret_cast<SD_Application*>(&app), &g_game_state);
      }

      app.clear_game_layers();
      app.scene_manager.clear();
      g_game_state = {};

      sd::ComponentFactory::clear();
      sd::ComponentFactory::register_default_pools();

      if (g_game_handle) {
        g_stale_handles.push_back(g_game_handle);
      }

      g_game_handle = new_handle;
      g_game_api    = new_api;

      g_game_api.on_load(reinterpret_cast<SD_Application*>(&app), &g_game_state);

      sd::log::engine::info("Game restart completed successfully");
    }

    // ── Shader reload (triggered from "Reload Shaders" button) ────────
    // The flag is consumed here so it doesn't accumulate; actual work is
    // deferred to the game's pipeline abstraction (hook consume_shader_reload_request()
    // when your rewrite is ready).
    if (app.consume_shader_reload_request()) {
      sd::log::engine::info("Shader reload requested — pipeline abstraction will handle this");
    }

    // ── Hot-reload (detect .so change, preserve scenes) ────────────────
    std::error_code ec{};
    if (!std::filesystem::exists(source_so, ec) || ec) {
      continue;
    }

    auto current_time{std::filesystem::last_write_time(source_so, ec)};
    if (ec) {
      sd::log::engine::warn("Failed to check write time: {}", ec.message());
      continue;
    }

    if (current_time > g_last_so_write_time) {
      // When paused, don't update g_last_so_write_time — on resume
      // we'll still detect the change.
      if (app.game_code_reload_paused) {
        continue;
      }

      // Cooldown prevents busy-looping on a failed dlopen.
      auto now{std::chrono::steady_clock::now()};
      if (now - g_last_reload_check < std::chrono::milliseconds(500)) {
        continue;
      }
      g_last_reload_check = now;

      g_last_so_write_time = current_time;

      sd::log::engine::info("Detected change, reloading game code...");

      void*   new_handle{};
      GameAPI new_api{};
      if (!load_game_code_safe(source_so, &new_handle, &new_api)) {
        sd::log::engine::error("Failed to load new game code - keeping old code running");
        continue;
      }

      sd::log::engine::info("Unloading old game code...");

      // Commands from the old .so must drain before we swap — after that,
      // the old handle stays mapped so all existing code pointers are valid.
      (void)app.services().vulkan.get_vulkan_device()->waitIdle();

      if (g_game_api.on_unload) {
        g_game_api.on_unload(reinterpret_cast<SD_Application*>(&app), &g_game_state);
      }

      app.clear_game_layers();

      // dlclose is intentionally NOT called here — outstanding function
      // pointers, lambdas, callbacks, and Vulkan debug hooks from the old
      // .so remain valid as long as the mapping is resident.
      if (g_game_handle) {
        g_stale_handles.push_back(g_game_handle);
      }

      g_game_handle = new_handle;
      g_game_api    = new_api;

      g_game_api.on_reload(reinterpret_cast<SD_Application*>(&app), &g_game_state);

      sd::log::engine::info("Game code reload completed successfully");
    }

    app.frame();
  }

  if (g_game_api.on_unload) {
    g_game_api.on_unload(reinterpret_cast<SD_Application*>(&app), &g_game_state);
  }

  if (g_game_handle) {
    g_stale_handles.push_back(g_game_handle);
  }
  for (auto* h : g_stale_handles) {
    dlclose(h);
  }

  return 0;
}

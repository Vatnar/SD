#include <chrono>
#include <cstring>
#include <dlfcn.h>
#include <filesystem>
#include <iostream>
#include <system_error>
#include <thread>

#include "Application.hpp"
#include "ConfigLoader.hpp"
#include "game.h"

static void* gameHandle = nullptr;
static GameAPI gameAPI = {};
static GameState gameState = {};
static std::filesystem::file_time_type lastSoWriteTime;
static int reloadCount = 0;

static std::string GetLiveSoName(const std::string& gameSo) {
  reloadCount++;
  return "./" + gameSo + "_live_" + std::to_string(reloadCount);
}

// Wait for file to stop changing (build system finished writing)
static bool WaitForFileStable(const std::filesystem::path& path,
                              std::chrono::milliseconds timeout = std::chrono::seconds(2),
                              std::chrono::milliseconds checkInterval = std::chrono::milliseconds(50)) {
  std::error_code ec;
  auto lastSize = std::filesystem::file_size(path, ec);
  if (ec) {
    SD::Log::Engine::Error("File stability check failed (file_size): {}", ec.message());
    return false;
  }
  auto lastTime = std::filesystem::last_write_time(path, ec);
  if (ec) {
    SD::Log::Engine::Error("File stability check failed (last_write_time): {}", ec.message());
    return false;
  }
  auto start = std::chrono::steady_clock::now();

  while (std::chrono::steady_clock::now() - start < timeout) {
    std::this_thread::sleep_for(checkInterval);

    auto currentSize = std::filesystem::file_size(path, ec);
    if (ec) {
      SD::Log::Engine::Error("File stability check failed (file_size): {}", ec.message());
      return false;
    }
    auto currentTime = std::filesystem::last_write_time(path, ec);
    if (ec) {
      SD::Log::Engine::Error("File stability check failed (last_write_time): {}", ec.message());
      return false;
    }

    if (currentSize == lastSize && currentTime == lastTime) {
      return true;  // File stable
    }

    lastSize = currentSize;
    lastTime = currentTime;
  }

  SD::Log::Engine::Warn("File {} still changing after {}ms, skipping reload", path.string(), timeout.count());
  return false;
}

// Load new game code while keeping old code mapped
// Returns true on success, false on failure (old code remains valid)
static bool LoadGameCodeSafe(const std::string& gameSoPath,
                             void** outNewHandle, GameAPI* outNewAPI) {
  // Wait for file to be stable (build finished)
  if (!WaitForFileStable(gameSoPath)) {
    return false;
  }

  std::string gameSoFilename = std::filesystem::path(gameSoPath).filename().string();
  std::string liveSo = GetLiveSoName(gameSoFilename);
  std::string tempSo = liveSo + ".tmp";

  // Copy to temp file first (not the watched file)
  std::error_code ec;
  if (!std::filesystem::copy_file(gameSoPath, tempSo,
                                  std::filesystem::copy_options::overwrite_existing, ec)) {
    SD::Log::Engine::Error("Failed to copy game code: {}", ec.message());
    return false;
  }

  // Atomically rename temp to final - this ensures the loader only sees complete file
  std::filesystem::rename(tempSo, liveSo, ec);
  if (ec) {
    SD::Log::Engine::Error("Failed to rename game code: {}", ec.message());
    return false;
  }

  // Load new code BEFORE closing old code
  void* newHandle = dlopen(liveSo.c_str(), RTLD_NOW);
  if (!newHandle) {
    const char* err = dlerror();
    SD::Log::Engine::Error("dlopen failed: {}", err ? err : "unknown error");
    return false;
  }

  auto* getAPI = reinterpret_cast<GetGameAPIFn>(dlsym(newHandle, GAME_GET_API_NAME));
  if (!getAPI) {
    const char* err = dlerror();
    SD::Log::Engine::Error("dlsym GetGameAPI failed: {}", err ? err : "unknown error");
    dlclose(newHandle);
    return false;
  }

  GameAPI newAPI = getAPI();
  if (!newAPI.OnLoad) {
    SD::Log::Engine::Error("Game code missing OnLoad function");
    dlclose(newHandle);
    return false;
  }

  std::printf("%p\t%p\t%p\n", newAPI.OnUpdate, newAPI.OnLoad, newAPI.OnUnload);
  SD::Log::Engine::Info("New game code loaded successfully");

  *outNewHandle = newHandle;
  *outNewAPI = newAPI;
  return true;
}

int main(int argc, char* argv[]) {
  ConfigLoader config(argc, argv);
  const auto& cfg = config.GetConfig();

  SD::Log::Init();
  SD::Log::Engine::Info("HotReloadApp starting");
  SD::Log::Engine::Info("  game-so-path: {}", cfg.gameSoPath);
  SD::Log::Engine::Info("  build-dir: {}", cfg.buildDir);
  SD::Log::Engine::Info("  app-name: {}", cfg.appName);
  SD::Log::Engine::Info("  window: {}x{}", cfg.windowWidth, cfg.windowHeight);

  // Initial load (old style - no fallback possible)
  {
    std::string gameSoFilename = std::filesystem::path(cfg.gameSoPath).filename().string();
    std::string liveSo = GetLiveSoName(gameSoFilename);
    std::string tempSo = liveSo + ".tmp";

    std::error_code ec;
    if (!std::filesystem::copy_file(cfg.gameSoPath, tempSo,
                                    std::filesystem::copy_options::overwrite_existing, ec)) {
      SD::Abort("Failed to copy game code: " + ec.message());
    }
    std::filesystem::rename(tempSo, liveSo, ec);
    if (ec) {
      SD::Abort("Failed to rename game code: " + ec.message());
    }

    gameHandle = dlopen(liveSo.c_str(), RTLD_NOW);
    if (!gameHandle) {
      SD::Log::Engine::Error("Failed to load game code: {}", dlerror());
      return 1;
    }

    auto* getAPI = reinterpret_cast<GetGameAPIFn>(dlsym(gameHandle, GAME_GET_API_NAME));
    if (!getAPI) {
      SD::Log::Engine::Error("Failed to get game API: {}", dlerror());
      return 1;
    }

    gameAPI = getAPI();
  }

  lastSoWriteTime = std::filesystem::last_write_time(cfg.gameSoPath);

  SD::Application app({cfg.appName.c_str(), cfg.windowWidth, cfg.windowHeight});

  // Initial load
  gameAPI.OnLoad(reinterpret_cast<SD_Application*>(&app), &gameState);

  // Run loop - check for .so changes
  while (app.IsRunning()) {
    // Check for reload (when user clicks Build in CLion)
    if (std::filesystem::exists(cfg.gameSoPath)) {
      auto currentTime = std::filesystem::last_write_time(cfg.gameSoPath);
      if (currentTime > lastSoWriteTime) {
        lastSoWriteTime = currentTime;

        SD::Log::Engine::Info("Detected change, reloading game code...");

        // Load NEW code first (while old code is still mapped)
        void* newHandle = nullptr;
        GameAPI newAPI = {};
        if (!LoadGameCodeSafe(cfg.gameSoPath, &newHandle, &newAPI)) {
          SD::Log::Engine::Error("Failed to load new game code - keeping old code running");
          continue;  // Don't reload, keep using current code
        }

        // New code loaded successfully - now safe to unload old
        SD::Log::Engine::Info("Unloading old game code...");

        // Unload old code
        if (gameAPI.OnUnload) {
          gameAPI.OnUnload(reinterpret_cast<SD_Application*>(&app), &gameState);
        }

        // CRITICAL: Wait for GPU to finish all work before destroying layers
        // This prevents use-after-free of Vulkan resources
        (void)app.GetVulkanContext().GetVulkanDevice()->waitIdle();

        // Clear engine state before loading new code
        app.ClearGameLayers();

        // Close old handle after layers are destroyed
        if (gameHandle) {
          dlclose(gameHandle);
        }

        // Switch to new code
        gameHandle = newHandle;
        gameAPI = newAPI;

        // Load new code with existing state
        gameAPI.OnLoad(reinterpret_cast<SD_Application*>(&app), &gameState);

        SD::Log::Engine::Info("Game code reload completed successfully");
      }
    }

    // Frame
    app.Frame();
  }

  // Cleanup
  if (gameAPI.OnUnload) {
    gameAPI.OnUnload(reinterpret_cast<SD_Application*>(&app), &gameState);
  }

  if (gameHandle) {
    dlclose(gameHandle);
  }

  return 0;
}

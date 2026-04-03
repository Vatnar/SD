#include <dlfcn.h>
#include <filesystem>
#include <iostream>
#include <cstring>

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

static bool LoadGameCode(const std::string& gameSoPath) {
    if (gameHandle) {
        dlclose(gameHandle);
        gameHandle = nullptr;
    }

    gameAPI = {};

    std::string gameSoFilename = std::filesystem::path(gameSoPath).filename().string();
    std::string liveSo = GetLiveSoName(gameSoFilename);
    
    try {
        if (!std::filesystem::copy_file(gameSoPath, liveSo,
                                        std::filesystem::copy_options::overwrite_existing))
            std::cerr << "Failed to copy " << gameSoPath << "\n";
    } catch (std::filesystem::filesystem_error& e) {
        SD::Abort("Failed with error " + std::string(e.what()));
    }

    gameHandle = dlopen(liveSo.c_str(), RTLD_NOW);
    if (!gameHandle) {
        std::cerr << "dlopen failed: " << dlerror() << '\n';
        return false;
    }

    auto* getAPI = reinterpret_cast<GetGameAPIFn>(dlsym(gameHandle, GAME_GET_API_NAME));
    if (!getAPI) {
        std::cerr << "dlsym GetGameAPI failed: " << dlerror() << '\n';
        dlclose(gameHandle);
        gameHandle = nullptr;
        return false;
    }

    gameAPI = getAPI();
    std::printf("%p\t%p\t%p\n", gameAPI.OnUpdate, gameAPI.OnLoad, gameAPI.OnUnload);
    std::cerr << "Game code loaded\n";
    return true;
}

int main(int argc, char* argv[]) {
    ConfigLoader config(argc, argv);
    const auto& cfg = config.GetConfig();

    SD::Log::Init();
    SPDLOG_INFO("HotReloadApp starting");
    SPDLOG_INFO("  game-so-path: {}", cfg.gameSoPath);
    SPDLOG_INFO("  build-dir: {}", cfg.buildDir);
    SPDLOG_INFO("  app-name: {}", cfg.appName);
    SPDLOG_INFO("  window: {}x{}", cfg.windowWidth, cfg.windowHeight);

    if (!LoadGameCode(cfg.gameSoPath)) {
        SPDLOG_ERROR("Failed to load game code");
        return 1;
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

                SPDLOG_INFO("Detected change, reloading game code...");

                // Unload old code
                if (gameAPI.OnUnload) {
                    gameAPI.OnUnload(reinterpret_cast<SD_Application*>(&app), &gameState);
                }

                // Clear engine state before reload
                app.ClearGameLayers();

                // Load new code
                if (LoadGameCode(cfg.gameSoPath)) {
                    // Reload with existing state
                    gameAPI.OnLoad(reinterpret_cast<SD_Application*>(&app), &gameState);
                } else {
                    SPDLOG_ERROR("Reload failed!");
                    break;
                }
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

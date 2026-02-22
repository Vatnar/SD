#include <dlfcn.h>
#include <filesystem>
#include <iostream>

#include "Application.hpp"
#include "game.h"

static void* gameHandle = nullptr;
static GameAPI gameAPI = {};
static GameState gameState = {};
static std::filesystem::file_time_type lastSoWriteTime;

static bool LoadGameCode() {
    // Close old handle first
    if (gameHandle) {
        dlclose(gameHandle);
        gameHandle = nullptr;
    }
    
    gameAPI = {};  // Clear function pointers
    
    gameHandle = dlopen("./libSandboxApp.so", RTLD_NOW);
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
    std::cerr << "Game code loaded\n";
    return true;
}

int main() {
    SD::Log::Init();
    
    if (!LoadGameCode()) {
        std::cerr << "Failed to load game code\n";
        return 1;
    }
    
    lastSoWriteTime = std::filesystem::last_write_time("./libSandboxApp.so");
    
    SD::Application app({"Sandbox", 1280, 720});
    
    // Initial load
    gameAPI.OnLoad(reinterpret_cast<SD_Application*>(&app), &gameState);
    
    // Run loop - check for .so changes
    while (app.IsRunning()) {
        // Check for reload
        if (std::filesystem::exists("./libSandboxApp.so")) {
            auto currentTime = std::filesystem::last_write_time("./libSandboxApp.so");
            if (currentTime > lastSoWriteTime) {
                lastSoWriteTime = currentTime;
                
                std::cerr << "\n=== RELOADING GAME CODE ===\n";
                
                // Unload old code
                gameAPI.OnUnload(reinterpret_cast<SD_Application*>(&app), &gameState);
                
                // Clear engine state before reload
                app.ClearGameLayers();
                
                // Load new code
                if (LoadGameCode()) {
                    // Reload with existing state
                    gameAPI.OnLoad(reinterpret_cast<SD_Application*>(&app), &gameState);
                } else {
                    std::cerr << "Reload failed!\n";
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

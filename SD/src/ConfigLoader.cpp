#include <cstring>
#include <iostream>
#include <stdlib.h>

#include "ConfigLoader.hpp"
#include "spdlog/spdlog.h"

ConfigLoader::ConfigLoader(int argc, char* argv[]) {
    LoadDefaults();

    mEngineDir = std::filesystem::path(SD_ENGINE_DIR);
    LoadEngineConfig(mEngineDir);

    LoadCwdConfig();
    LoadEnvConfig();
    if (argc > 0) {
        LoadCliConfig(argc, argv);
    }
    
    ResolvePaths();
}

void ConfigLoader::LoadDefaults() {
    mConfig.buildDir = "../build";
    mConfig.gameSoPath = "libSandboxApp.so";
    mConfig.appName = "Sandbox";
    mConfig.windowWidth = 1280;
    mConfig.windowHeight = 720;
    mConfig.pollInterval = 0.5f;
}

void ConfigLoader::LoadEngineConfig(const std::filesystem::path& engineDir) {
    auto engineConfigPath = engineDir / "config" / "engine.toml";
    if (std::filesystem::exists(engineConfigPath)) {
        try {
            toml::table config = toml::parse_file(engineConfigPath.string());
            
            if (auto* engine = config["engine"].as_table()) {
                if (auto* buildDir = engine->get("build-dir")) {
                    mConfig.buildDir = buildDir->value<std::string>().value_or(mConfig.buildDir);
                }
                if (auto* windowWidth = engine->get("window-width")) {
                    mConfig.windowWidth = windowWidth->value<int>().value_or(mConfig.windowWidth);
                }
                if (auto* windowHeight = engine->get("window-height")) {
                    mConfig.windowHeight = windowHeight->value<int>().value_or(mConfig.windowHeight);
                }
                if (auto* pollInterval = engine->get("poll-interval")) {
                    mConfig.pollInterval = pollInterval->value<float>().value_or(mConfig.pollInterval);
                }
            }
            
            SPDLOG_INFO("Loaded engine config from {}", engineConfigPath.string());
        } catch (const toml::parse_error& e) {
            SPDLOG_ERROR("Failed to parse engine config: {}", e.what());
        }
    }
}

void ConfigLoader::LoadCwdConfig() {
    auto cwdConfigPath = std::filesystem::current_path() / "hotreload.toml";
    if (std::filesystem::exists(cwdConfigPath)) {
        OverrideFromFile(cwdConfigPath);
    }
}

void ConfigLoader::OverrideFromFile(const std::filesystem::path& path) {
    try {
        toml::table config = toml::parse_file(path.string());
        
        if (auto* hotreload = config["hotreload"].as_table()) {
            if (auto* gameSoPath = hotreload->get("game-so-path")) {
                mConfig.gameSoPath = gameSoPath->value<std::string>().value_or(mConfig.gameSoPath);
            }
            if (auto* appName = hotreload->get("app-name")) {
                mConfig.appName = appName->value<std::string>().value_or(mConfig.appName);
            }
            if (auto* windowWidth = hotreload->get("window-width")) {
                mConfig.windowWidth = windowWidth->value<int>().value_or(mConfig.windowWidth);
            }
            if (auto* windowHeight = hotreload->get("window-height")) {
                mConfig.windowHeight = windowHeight->value<int>().value_or(mConfig.windowHeight);
            }
            if (auto* buildDir = hotreload->get("build-dir")) {
                mConfig.buildDir = buildDir->value<std::string>().value_or(mConfig.buildDir);
            }
        }
        
        SPDLOG_INFO("Loaded config from {}", path.string());
    } catch (const toml::parse_error& e) {
        SPDLOG_ERROR("Failed to parse config {}: {}", path.string(), e.what());
    }
}

void ConfigLoader::LoadEnvConfig() {
    if (const char* env = std::getenv("SD_GAME_SO_PATH")) {
        mConfig.gameSoPath = env;
    }
    if (const char* env = std::getenv("SD_APP_NAME")) {
        mConfig.appName = env;
    }
    if (const char* env = std::getenv("SD_BUILD_DIR")) {
        mConfig.buildDir = env;
    }
    if (const char* env = std::getenv("SD_WINDOW_WIDTH")) {
        mConfig.windowWidth = std::atoi(env);
    }
    if (const char* env = std::getenv("SD_WINDOW_HEIGHT")) {
        mConfig.windowHeight = std::atoi(env);
    }
}

void ConfigLoader::LoadCliConfig(int argc, char* argv[]) {
    OverrideFromCli(argc, argv);
}

void ConfigLoader::OverrideFromCli(int argc, char* argv[]) {
    if (argc == 0) return;
    
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--game-so-path") == 0 && i + 1 < argc) {
            mConfig.gameSoPath = argv[++i];
        } else if (strcmp(argv[i], "--build-dir") == 0 && i + 1 < argc) {
            mConfig.buildDir = argv[++i];
        } else if (strcmp(argv[i], "--app-name") == 0 && i + 1 < argc) {
            mConfig.appName = argv[++i];
        } else if (strcmp(argv[i], "--window-width") == 0 && i + 1 < argc) {
            mConfig.windowWidth = std::atoi(argv[++i]);
        } else if (strcmp(argv[i], "--window-height") == 0 && i + 1 < argc) {
            mConfig.windowHeight = std::atoi(argv[++i]);
        }
    }
}

void ConfigLoader::ResolvePaths() {
    // Resolve gameSoPath relative to CURRENT WORKING DIRECTORY (CWD)
    // This is where you run HotReloadApp from
    std::filesystem::path cwd = std::filesystem::current_path();
    std::filesystem::path gameSoPath = std::filesystem::path(mConfig.gameSoPath);
    
    if (gameSoPath.is_absolute()) {
        mConfig.gameSoPath = gameSoPath.lexically_normal().string();
    } else {
        // Handle "." as empty (current directory)
        if (gameSoPath == std::filesystem::path(".")) {
            gameSoPath = std::filesystem::path("");
        }
        mConfig.gameSoPath = (cwd / gameSoPath).lexically_normal().string();
    }
    
    SPDLOG_INFO("Resolved game-so path: {}", mConfig.gameSoPath);
}

#pragma once

#include <string>
#include <optional>
#include <filesystem>

#include <toml++/toml.h>

struct HotReloadConfig {
    std::string buildDir = "../build";
    std::string gameSoPath = "libSandboxApp.so";
    std::string appName = "Sandbox";
    int windowWidth = 1280;
    int windowHeight = 720;
    float pollInterval = 0.5f;
};

class ConfigLoader {
public:
    ConfigLoader(int argc = 0, char* argv[] = nullptr);

    const HotReloadConfig& GetConfig() const { return mConfig; }

private:
    void LoadDefaults();
    void LoadEngineConfig(const std::filesystem::path& engineDir);
    void LoadCwdConfig();
    void LoadEnvConfig();
    void LoadCliConfig(int argc, char* argv[]);

    void OverrideFromFile(const std::filesystem::path& path);
    void OverrideFromEnv();
    void OverrideFromCli(int argc, char* argv[]);
    
    void ResolvePaths();

    HotReloadConfig mConfig;
    std::filesystem::path mEngineDir;
};

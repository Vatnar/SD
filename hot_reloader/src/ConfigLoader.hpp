#pragma once

#include <filesystem>
#include <string>


namespace hr {

struct HotReloadConfig {
  std::string build_dir     = "../build";
  std::string game_so_path  = "libSandboxApp.so";
  std::string app_name      = "Sandbox";
  int         window_width  = 1280;
  int         window_height = 720;
  float       poll_interval = 0.5f;
};

class ConfigLoader {
public:
  ConfigLoader(int argc = 0, char** argv = nullptr);

  const HotReloadConfig& get_config() const { return m_config; }

private:
  void load_defaults();
  void load_engine_config();
  void load_cwd_config();
  void load_exe_dir_config();
  void load_env_config();
  void load_cli_config(int argc, char** argv);

  void override_from_file(const std::filesystem::path& path);
  void override_from_env();
  void override_from_cli(int argc, char** argv);

  void resolve_paths();

  HotReloadConfig       m_config;
  std::filesystem::path m_exe_dir;
};
} // namespace hr

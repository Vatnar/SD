#include "ConfigLoader.hpp"

#include <cstring>

#include <SD/core/logging.hpp>
#include <SD/core/types.hpp>
#include <toml++/toml.hpp>

#if defined(__linux__)
#include <unistd.h>

#include <linux/limits.h>
#endif

namespace {
std::filesystem::path get_executable_dir() {
#if defined(__linux__)
  char    buf[PATH_MAX];
  ssize_t len{readlink("/proc/self/exe", buf, sizeof(buf) - 1)};
  if (len > 0) {
    buf[len] = '\0';
    return std::filesystem::path(buf).parent_path();
  }
#endif
  return std::filesystem::current_path();
}
} // namespace

namespace hr {
ConfigLoader::ConfigLoader(int argc, char* argv[]) {
  load_defaults();

  load_engine_config();

  m_exe_dir = get_executable_dir();
  load_exe_dir_config();
  load_cwd_config();
  load_env_config();
  if (argc > 0) {
    load_cli_config(argc, argv);
  }

  resolve_paths();
}

void ConfigLoader::load_defaults() {
  m_config = {
      .build_dir     = "../build",
      .game_so_path  = "libSandboxApp.so",
      .app_name      = "Sandbox",
      .window_width  = 1280,
      .window_height = 720,
      .poll_interval = 0.5f,
  };
}

void ConfigLoader::load_engine_config() {
  std::string engine_config_path{"engine.toml"};
  if (std::filesystem::exists(engine_config_path)) {
    toml::parse_result config{toml::parse_file(engine_config_path)};
    if (!config) {
      sd::log::engine::error("Failed to parse engine config: {}", config.error().description());
      return;
    }

    toml::table& engine_table{config.table()};
    if (auto* engine{engine_table["engine"].as_table()}) {
      if (auto* build_dir{engine->get("build-dir")}) {
        m_config.build_dir = build_dir->value<std::string>().value_or(m_config.build_dir);
      }
      if (auto* window_width{engine->get("window-width")}) {
        m_config.window_width = window_width->value<int>().value_or(m_config.window_width);
      }
      if (auto* window_height{engine->get("window-height")}) {
        m_config.window_height = window_height->value<int>().value_or(m_config.window_height);
      }
      if (auto* poll_interval{engine->get("poll-interval")}) {
        m_config.poll_interval = poll_interval->value<float>().value_or(m_config.poll_interval);
      }
    }

    sd::log::engine::info("Loaded engine config from {}", engine_config_path);
  }
}

void ConfigLoader::load_cwd_config() {
  auto cwd_config_path{std::filesystem::current_path() / "hotreload.toml"};
  if (std::filesystem::exists(cwd_config_path)) {
    override_from_file(cwd_config_path);
  }
}

void ConfigLoader::load_exe_dir_config() {
  auto exe_config_path{m_exe_dir / "hotreload.toml"};
  if (std::filesystem::exists(exe_config_path)) {
    override_from_file(exe_config_path);
  }
}

void ConfigLoader::override_from_file(const std::filesystem::path& path) {
  toml::parse_result config{toml::parse_file(path.string())};
  if (!config) {
    sd::log::engine::error("Failed to parse config {}: {}",
                           path.string(),
                           config.error().description());
    return;
  }

  auto& hotreload_table{config.table()};
  if (auto* hotreload{hotreload_table["hotreload"].as_table()}) {
    if (auto* game_so_path{hotreload->get("game-so-path")}) {
      m_config.game_so_path = game_so_path->value<std::string>().value_or(m_config.game_so_path);
    }
    if (auto* app_name{hotreload->get("app-name")}) {
      m_config.app_name = app_name->value<std::string>().value_or(m_config.app_name);
    }
    if (auto* window_width{hotreload->get("window-width")}) {
      m_config.window_width = window_width->value<int>().value_or(m_config.window_width);
    }
    if (auto* window_height{hotreload->get("window-height")}) {
      m_config.window_height = window_height->value<int>().value_or(m_config.window_height);
    }
    if (auto* build_dir{hotreload->get("build-dir")}) {
      m_config.build_dir = build_dir->value<std::string>().value_or(m_config.build_dir);
    }
  }

  sd::log::engine::info("Loaded config from {}", path.string());
}

void ConfigLoader::load_env_config() {
  if (const char* env{std::getenv("SD_GAME_SO_PATH")}) {
    m_config.game_so_path = env;
  }
  if (const char* env{std::getenv("SD_APP_NAME")}) {
    m_config.app_name = env;
  }
  if (const char* env{std::getenv("SD_BUILD_DIR")}) {
    m_config.build_dir = env;
  }
  if (const char* env{std::getenv("SD_WINDOW_WIDTH")}) {
    m_config.window_width = std::atoi(env);
  }
  if (const char* env{std::getenv("SD_WINDOW_HEIGHT")}) {
    m_config.window_height = std::atoi(env);
  }
}

void ConfigLoader::load_cli_config(int argc, char** argv) {
  override_from_cli(argc, argv);
}

void ConfigLoader::override_from_cli(int argc, char** argv) {
  if (argc == 0)
    return;

  for (USize i{1}; i < argc; i++) {
    if (strcmp(argv[i], "--game-so-path") == 0 && i + 1 < argc) {
      m_config.game_so_path = argv[++i];
    } else if (strcmp(argv[i], "--build-dir") == 0 && i + 1 < argc) {
      m_config.build_dir = argv[++i];
    } else if (strcmp(argv[i], "--app-name") == 0 && i + 1 < argc) {
      m_config.app_name = argv[++i];
    } else if (strcmp(argv[i], "--window-width") == 0 && i + 1 < argc) {
      m_config.window_width = std::atoi(argv[++i]);
    } else if (strcmp(argv[i], "--window-height") == 0 && i + 1 < argc) {
      m_config.window_height = std::atoi(argv[++i]);
    }
  }
}

void ConfigLoader::resolve_paths() {
  std::filesystem::path game_so_path = std::filesystem::path(m_config.game_so_path);

  if (game_so_path.is_absolute()) {
    m_config.game_so_path = game_so_path.lexically_normal().string();
  } else {
    m_config.game_so_path = (m_exe_dir / game_so_path).lexically_normal().string();
  }

  sd::log::engine::info("Resolved game-so path: {}", m_config.game_so_path);
}
} // namespace hr

#pragma once

#include <chrono>
#include <deque>
#include <imgui.h>
#include <memory>
#include <spdlog/common.h>
#include <spdlog/spdlog.h>
#include <string>
#include <utility>
#include <vector>

#define ENGINE_LOG_LEVEL_TRACE 0
#define ENGINE_LOG_LEVEL_DEBUG 1
#define ENGINE_LOG_LEVEL_INFO 2
#define ENGINE_LOG_LEVEL_WARN 3
#define ENGINE_LOG_LEVEL_ERROR 4
#define ENGINE_LOG_LEVEL_CRITICAL 5
#define ENGINE_LOG_LEVEL_OFF 6

#ifndef ENGINE_LOG_LEVEL
#define ENGINE_LOG_LEVEL ENGINE_LOG_LEVEL_INFO
#endif

#ifndef ENGINE_LOG_LEVEL_ENGINE
#define ENGINE_LOG_LEVEL_ENGINE ENGINE_LOG_LEVEL
#endif

#ifndef ENGINE_LOG_LEVEL_GAME
#define ENGINE_LOG_LEVEL_GAME ENGINE_LOG_LEVEL
#endif

namespace sd::log {

enum class LogLevel {
  TRACE = ENGINE_LOG_LEVEL_TRACE,
  DEBUG = ENGINE_LOG_LEVEL_DEBUG,
  INFO = ENGINE_LOG_LEVEL_INFO,
  WARN = ENGINE_LOG_LEVEL_WARN,
  ERROR = ENGINE_LOG_LEVEL_ERROR,
  CRITICAL = ENGINE_LOG_LEVEL_CRITICAL,
  OFF = ENGINE_LOG_LEVEL_OFF
};

struct LogEntry {
  std::string category;
  LogLevel level;
  std::string message;
  float uptime_sec{};
};

struct CategoryInfo {
  std::string name;
  bool visible = true;
  ImVec4 color{};
};

std::deque<LogEntry> get_log_history();
void add_log_entry(LogEntry entry);

std::vector<CategoryInfo>& get_category_registry();
void register_category(const char* name, ImVec4 color);

bool is_category_under(const std::string& child, const std::string& parent);

/**
 * @brief Initializes all category loggers, shared sinks, and the ImGui sink
 */
void init();

spdlog::level::level_enum to_spdlog_level(LogLevel level);

LogLevel from_spdlog_level(spdlog::level::level_enum level);

} // namespace SD::Log


// TODO: Try to do this without macro
#define SD_LOG_CATEGORY_IMPL(CategoryPath, Level)                              \
  constexpr auto cMinLevel = static_cast<::sd::log::LogLevel>(Level);          \
  constexpr const char* cCategoryPath = CategoryPath;                          \
                                                                               \
  template<typename... Args>                                                   \
  inline void trace(spdlog::format_string_t<Args...> fmt, Args&&... args) {    \
    if constexpr (cMinLevel <= ::sd::log::LogLevel::TRACE) {                   \
      if (auto logger = spdlog::get(CategoryPath)) {                           \
        logger->trace(fmt, std::forward<Args>(args)...);                       \
      }                                                                        \
    }                                                                          \
  }                                                                            \
                                                                               \
  template<typename... Args>                                                   \
  inline void debug(spdlog::format_string_t<Args...> fmt, Args&&... args) {    \
    if constexpr (cMinLevel <= ::sd::log::LogLevel::DEBUG) {                   \
      if (auto logger = spdlog::get(CategoryPath)) {                           \
        logger->debug(fmt, std::forward<Args>(args)...);                       \
      }                                                                        \
    }                                                                          \
  }                                                                            \
                                                                               \
  template<typename... Args>                                                   \
  inline void info(spdlog::format_string_t<Args...> fmt, Args&&... args) {     \
    if constexpr (cMinLevel <= ::sd::log::LogLevel::INFO) {                    \
      if (auto logger = spdlog::get(CategoryPath)) {                           \
        logger->info(fmt, std::forward<Args>(args)...);                        \
      }                                                                        \
    }                                                                          \
  }                                                                            \
                                                                               \
  template<typename... Args>                                                   \
  inline void warn(spdlog::format_string_t<Args...> fmt, Args&&... args) {     \
    if constexpr (cMinLevel <= ::sd::log::LogLevel::WARN) {                    \
      if (auto logger = spdlog::get(CategoryPath)) {                           \
        logger->warn(fmt, std::forward<Args>(args)...);                        \
      }                                                                        \
    }                                                                          \
  }                                                                            \
                                                                               \
  template<typename... Args>                                                   \
  inline void error(spdlog::format_string_t<Args...> fmt, Args&&... args) {    \
    if constexpr (cMinLevel <= ::sd::log::LogLevel::ERROR) {                   \
      if (auto logger = spdlog::get(CategoryPath)) {                           \
        logger->error(fmt, std::forward<Args>(args)...);                       \
      }                                                                        \
    }                                                                          \
  }                                                                            \
                                                                               \
  template<typename... Args>                                                   \
  inline void critical(spdlog::format_string_t<Args...> fmt, Args&&... args) { \
    if constexpr (cMinLevel <= ::sd::log::LogLevel::CRITICAL) {                \
      if (auto logger = spdlog::get(CategoryPath)) {                           \
        logger->critical(fmt, std::forward<Args>(args)...);                    \
      }                                                                        \
    }                                                                          \
  }


namespace sd::log::engine {
SD_LOG_CATEGORY_IMPL("Engine", ENGINE_LOG_LEVEL_ENGINE)

namespace Renderer {
SD_LOG_CATEGORY_IMPL("Engine/Renderer", ENGINE_LOG_LEVEL_ENGINE)
} // namespace Renderer

namespace ECS {
SD_LOG_CATEGORY_IMPL("Engine/ECS", ENGINE_LOG_LEVEL_ENGINE)
} // namespace ECS

namespace Network {
SD_LOG_CATEGORY_IMPL("Engine/Network", ENGINE_LOG_LEVEL_ENGINE)
} // namespace Network
} // namespace SD::Log::Engine

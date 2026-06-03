#pragma once

#include <chrono>
#include <deque>
#include <imgui.h>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <SD/export.hpp>
#include <spdlog/common.h>
#include <spdlog/spdlog.h>

#define ENGINE_LOG_LEVEL_TRACE    0
#define ENGINE_LOG_LEVEL_DEBUG    1
#define ENGINE_LOG_LEVEL_INFO     2
#define ENGINE_LOG_LEVEL_WARN     3
#define ENGINE_LOG_LEVEL_ERROR    4
#define ENGINE_LOG_LEVEL_CRITICAL 5
#define ENGINE_LOG_LEVEL_OFF      6

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
  TRACE    = ENGINE_LOG_LEVEL_TRACE,
  DEBUG    = ENGINE_LOG_LEVEL_DEBUG,
  INFO     = ENGINE_LOG_LEVEL_INFO,
  WARN     = ENGINE_LOG_LEVEL_WARN,
  ERROR    = ENGINE_LOG_LEVEL_ERROR,
  CRITICAL = ENGINE_LOG_LEVEL_CRITICAL,
  OFF      = ENGINE_LOG_LEVEL_OFF
};

struct LogEntry {
  std::string category;
  LogLevel    level;
  std::string message;
  float       uptime_sec{};
};

struct CategoryInfo {
  std::string name;
  bool        visible = true;
  ImVec4      color{};
};

SD_EXPORT std::deque<LogEntry> get_log_history();
SD_EXPORT void                 add_log_entry(LogEntry entry);

SD_EXPORT std::vector<CategoryInfo>& get_category_registry();
SD_EXPORT void                       register_category(const char* name, ImVec4 color);

bool is_category_under(const std::string& child, const std::string& parent);

/**
 * @brief Initializes all category loggers, shared sinks, and the ImGui sink
 */
SD_EXPORT void init();

spdlog::level::level_enum to_spdlog_level(LogLevel level);

LogLevel from_spdlog_level(spdlog::level::level_enum level);

inline std::shared_ptr<spdlog::logger> get_category_logger_or_report(const char* categoryPath) {
  if (auto logger = spdlog::get(categoryPath)) {
    return logger;
  }

  spdlog::error("Log category '{}' is not registered. Message was not logged.", categoryPath);
  return nullptr;
}

} // namespace sd::log


#define SD_LOG_CATEGORY_IMPL(CategoryPath, Level)                                 \
  constexpr auto        cMinLevel     = static_cast<::sd::log::LogLevel>(Level);  \
  constexpr const char* cCategoryPath = CategoryPath;                             \
                                                                                  \
  template<typename... Args>                                                      \
  inline void trace(spdlog::format_string_t<Args...> fmt, Args&&... args) {       \
    if constexpr (cMinLevel <= ::sd::log::LogLevel::TRACE) {                      \
      if (auto logger = ::sd::log::get_category_logger_or_report(CategoryPath)) { \
        logger->trace(fmt, std::forward<Args>(args)...);                          \
      }                                                                           \
    }                                                                             \
  }                                                                               \
                                                                                  \
  template<typename... Args>                                                      \
  inline void debug(spdlog::format_string_t<Args...> fmt, Args&&... args) {       \
    if constexpr (cMinLevel <= ::sd::log::LogLevel::DEBUG) {                      \
      if (auto logger = ::sd::log::get_category_logger_or_report(CategoryPath)) { \
        logger->debug(fmt, std::forward<Args>(args)...);                          \
      }                                                                           \
    }                                                                             \
  }                                                                               \
                                                                                  \
  template<typename... Args>                                                      \
  inline void info(spdlog::format_string_t<Args...> fmt, Args&&... args) {        \
    if constexpr (cMinLevel <= ::sd::log::LogLevel::INFO) {                       \
      if (auto logger = ::sd::log::get_category_logger_or_report(CategoryPath)) { \
        logger->info(fmt, std::forward<Args>(args)...);                           \
      }                                                                           \
    }                                                                             \
  }                                                                               \
                                                                                  \
  template<typename... Args>                                                      \
  inline void warn(spdlog::format_string_t<Args...> fmt, Args&&... args) {        \
    if constexpr (cMinLevel <= ::sd::log::LogLevel::WARN) {                       \
      if (auto logger = ::sd::log::get_category_logger_or_report(CategoryPath)) { \
        logger->warn(fmt, std::forward<Args>(args)...);                           \
      }                                                                           \
    }                                                                             \
  }                                                                               \
                                                                                  \
  template<typename... Args>                                                      \
  inline void error(spdlog::format_string_t<Args...> fmt, Args&&... args) {       \
    if constexpr (cMinLevel <= ::sd::log::LogLevel::ERROR) {                      \
      if (auto logger = ::sd::log::get_category_logger_or_report(CategoryPath)) { \
        logger->error(fmt, std::forward<Args>(args)...);                          \
      }                                                                           \
    }                                                                             \
  }                                                                               \
                                                                                  \
  template<typename... Args>                                                      \
  inline void critical(spdlog::format_string_t<Args...> fmt, Args&&... args) {    \
    if constexpr (cMinLevel <= ::sd::log::LogLevel::CRITICAL) {                   \
      if (auto logger = ::sd::log::get_category_logger_or_report(CategoryPath)) { \
        logger->critical(fmt, std::forward<Args>(args)...);                       \
      }                                                                           \
    }                                                                             \
  }


namespace sd::log::engine {
SD_LOG_CATEGORY_IMPL("engine", ENGINE_LOG_LEVEL_ENGINE)

namespace renderer {
SD_LOG_CATEGORY_IMPL("engine/renderer", ENGINE_LOG_LEVEL_ENGINE)
} // namespace renderer

namespace ecs {
SD_LOG_CATEGORY_IMPL("engine/ecs", ENGINE_LOG_LEVEL_ENGINE)
} // namespace ecs

namespace network {
SD_LOG_CATEGORY_IMPL("engine/network", ENGINE_LOG_LEVEL_ENGINE)
} // namespace network
} // namespace sd::log::engine

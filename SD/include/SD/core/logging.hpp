#pragma once

#include <chrono>
#include <deque>
#include <imgui.h>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <SD/export.hpp>
#include <fmt/format.h>
#include <quill/Backend.h>
#include <quill/LogFunctions.h>
#include <quill/core/LogLevel.h>

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
  OFF      = ENGINE_LOG_LEVEL_OFF,
  GENERAL  = 7
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
SD_EXPORT void                 clear_history();
SD_EXPORT size_t               get_total_entry_count();
SD_EXPORT std::vector<LogEntry> get_entries(size_t offset, size_t count);

SD_EXPORT std::vector<CategoryInfo>& get_category_registry();
SD_EXPORT void                       register_category(const char* name, ImVec4 color);

bool is_category_under(const std::string& child, const std::string& parent);

SD_EXPORT void init();

quill::LogLevel to_quill_level(LogLevel level);

LogLevel from_quill_level(quill::LogLevel level);

quill::Logger* get_category_logger_or_report(const char* categoryPath);

} // namespace sd::log

#define SD_LOG_CATEGORY_IMPL(CategoryPath, Level)                                             \
  constexpr auto        cMinLevel     = static_cast<::sd::log::LogLevel>(Level);              \
  constexpr const char* cCategoryPath = CategoryPath;                                         \
                                                                                              \
  inline ::quill::Logger* get_logger() {                                                      \
    static ::quill::Logger* logger = ::sd::log::get_category_logger_or_report(CategoryPath);  \
    return logger;                                                                            \
  }                                                                                           \
                                                                                              \
  template<typename... Args>                                                                  \
  inline void trace(fmt::format_string<Args...> fmt, Args&&... args) {                        \
    if constexpr (cMinLevel <= ::sd::log::LogLevel::TRACE) {                                  \
      if (auto* logger = get_logger()) {                                                      \
        ::quill::tracel3(logger, fmt.get().data(), std::forward<Args>(args)...);              \
      }                                                                                       \
    }                                                                                         \
  }                                                                                           \
                                                                                              \
  template<typename... Args>                                                                  \
  inline void debug(fmt::format_string<Args...> fmt, Args&&... args) {                        \
    if constexpr (cMinLevel <= ::sd::log::LogLevel::DEBUG) {                                  \
      if (auto* logger = get_logger()) {                                                      \
        ::quill::debug(logger, fmt.get().data(), std::forward<Args>(args)...);                \
      }                                                                                       \
    }                                                                                         \
  }                                                                                           \
                                                                                              \
  template<typename... Args>                                                                  \
  inline void info(fmt::format_string<Args...> fmt, Args&&... args) {                         \
    if constexpr (cMinLevel <= ::sd::log::LogLevel::INFO) {                                   \
      if (auto* logger = get_logger()) {                                                      \
        ::quill::info(logger, fmt.get().data(), std::forward<Args>(args)...);                 \
      }                                                                                       \
    }                                                                                         \
  }                                                                                           \
                                                                                              \
  template<typename... Args>                                                                  \
  inline void warn(fmt::format_string<Args...> fmt, Args&&... args) {                         \
    if constexpr (cMinLevel <= ::sd::log::LogLevel::WARN) {                                   \
      if (auto* logger = get_logger()) {                                                      \
        ::quill::warning(logger, fmt.get().data(), std::forward<Args>(args)...);              \
      }                                                                                       \
    }                                                                                         \
  }                                                                                           \
                                                                                              \
  template<typename... Args>                                                                  \
  inline void error(fmt::format_string<Args...> fmt, Args&&... args) {                        \
    if (auto* logger = get_logger()) {                                                        \
      ::quill::error(logger, fmt.get().data(), std::forward<Args>(args)...);                  \
    }                                                                                         \
  }                                                                                           \
                                                                                              \
  template<typename... Args>                                                                  \
  [[noreturn]] inline void critical(fmt::format_string<Args...> fmt, Args&&... args) {        \
    if (auto* logger = get_logger()) {                                                        \
      ::quill::critical(logger, fmt.get().data(), std::forward<Args>(args)...);               \
      logger->flush_log();                                                                    \
    }                                                                                         \
    ::quill::Backend::stop();                                                                 \
    std::abort();                                                                             \
  }                                                                                           \
                                                                                              \
  template<typename... Args>                                                                  \
  inline void general(Args&&... args) {                                                       \
    info(std::forward<Args>(args)...);                                                        \
  }                                                                                           \
                                                                                              \
  template<typename... Args>                                                                  \
  inline void tagged(std::string_view            subcategory,                                 \
                     fmt::format_string<Args...> fmt,                                         \
                     Args&&... args) {                                                        \
    if (auto* logger = get_logger()) {                                                        \
      auto msg =                                                                              \
          fmt::format("[{}] {}", subcategory, fmt::format(fmt, std::forward<Args>(args)...)); \
      ::quill::info(logger, msg.c_str());                                                     \
    }                                                                                         \
  }


namespace sd::log {
namespace engine {
SD_LOG_CATEGORY_IMPL("engine", ENGINE_LOG_LEVEL_ENGINE)

namespace profiler {
SD_LOG_CATEGORY_IMPL("profiler", ENGINE_LOG_LEVEL_DEBUG)
}
namespace shader {
SD_LOG_CATEGORY_IMPL("engine/shader", ENGINE_LOG_LEVEL_ENGINE)
}
namespace renderer {
SD_LOG_CATEGORY_IMPL("engine/renderer", ENGINE_LOG_LEVEL_ENGINE)
} // namespace renderer

namespace ecs {
SD_LOG_CATEGORY_IMPL("engine/ecs", ENGINE_LOG_LEVEL_ENGINE)
} // namespace ecs

namespace network {
SD_LOG_CATEGORY_IMPL("engine/network", ENGINE_LOG_LEVEL_ENGINE)
} // namespace network
} // namespace engine
namespace vulkan {
SD_LOG_CATEGORY_IMPL("vulkan", ENGINE_LOG_LEVEL_ENGINE)
}
namespace debug_layer {
SD_LOG_CATEGORY_IMPL("debug_layer", ENGINE_LOG_LEVEL_ENGINE)
}
} // namespace sd::log

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

namespace SD::Log {

enum class LogLevel {
  Trace = ENGINE_LOG_LEVEL_TRACE,
  Debug = ENGINE_LOG_LEVEL_DEBUG,
  Info = ENGINE_LOG_LEVEL_INFO,
  Warn = ENGINE_LOG_LEVEL_WARN,
  Error = ENGINE_LOG_LEVEL_ERROR,
  Critical = ENGINE_LOG_LEVEL_CRITICAL,
  Off = ENGINE_LOG_LEVEL_OFF
};

struct LogEntry {
  std::string category;
  LogLevel level;
  std::string message;
  float uptimeSec{};
};

struct CategoryInfo {
  std::string name;
  bool visible = true;
  ImVec4 color{};
};

const std::deque<LogEntry>& GetLogHistory();
void AddLogEntry(LogEntry entry);

std::vector<CategoryInfo>& GetCategoryRegistry();
void RegisterCategory(const char* name, ImVec4 color);

bool IsCategoryUnder(const std::string& child, const std::string& parent);

/**
 * @brief Initializes all category loggers, shared sinks, and the ImGui sink
 */
void Init();

spdlog::level::level_enum ToSpdlogLevel(LogLevel level);

LogLevel FromSpdlogLevel(spdlog::level::level_enum level);

} // namespace SD::Log


#define SD_LOG_CATEGORY_IMPL(CategoryPath, Level)                              \
  constexpr auto cMinLevel = static_cast<::SD::Log::LogLevel>(Level);          \
  constexpr const char* cCategoryPath = CategoryPath;                          \
                                                                               \
  template<typename... Args>                                                   \
  inline void Trace(spdlog::format_string_t<Args...> fmt, Args&&... args) {    \
    if constexpr (cMinLevel <= ::SD::Log::LogLevel::Trace) {                   \
      if (auto logger = spdlog::get(CategoryPath)) {                           \
        logger->trace(fmt, std::forward<Args>(args)...);                       \
      }                                                                        \
    }                                                                          \
  }                                                                            \
                                                                               \
  template<typename... Args>                                                   \
  inline void Debug(spdlog::format_string_t<Args...> fmt, Args&&... args) {    \
    if constexpr (cMinLevel <= ::SD::Log::LogLevel::Debug) {                   \
      if (auto logger = spdlog::get(CategoryPath)) {                           \
        logger->debug(fmt, std::forward<Args>(args)...);                       \
      }                                                                        \
    }                                                                          \
  }                                                                            \
                                                                               \
  template<typename... Args>                                                   \
  inline void Info(spdlog::format_string_t<Args...> fmt, Args&&... args) {     \
    if constexpr (cMinLevel <= ::SD::Log::LogLevel::Info) {                    \
      if (auto logger = spdlog::get(CategoryPath)) {                           \
        logger->info(fmt, std::forward<Args>(args)...);                        \
      }                                                                        \
    }                                                                          \
  }                                                                            \
                                                                               \
  template<typename... Args>                                                   \
  inline void Warn(spdlog::format_string_t<Args...> fmt, Args&&... args) {     \
    if constexpr (cMinLevel <= ::SD::Log::LogLevel::Warn) {                    \
      if (auto logger = spdlog::get(CategoryPath)) {                           \
        logger->warn(fmt, std::forward<Args>(args)...);                        \
      }                                                                        \
    }                                                                          \
  }                                                                            \
                                                                               \
  template<typename... Args>                                                   \
  inline void Error(spdlog::format_string_t<Args...> fmt, Args&&... args) {    \
    if constexpr (cMinLevel <= ::SD::Log::LogLevel::Error) {                   \
      if (auto logger = spdlog::get(CategoryPath)) {                           \
        logger->error(fmt, std::forward<Args>(args)...);                       \
      }                                                                        \
    }                                                                          \
  }                                                                            \
                                                                               \
  template<typename... Args>                                                   \
  inline void Critical(spdlog::format_string_t<Args...> fmt, Args&&... args) { \
    if constexpr (cMinLevel <= ::SD::Log::LogLevel::Critical) {                \
      if (auto logger = spdlog::get(CategoryPath)) {                           \
        logger->critical(fmt, std::forward<Args>(args)...);                    \
      }                                                                        \
    }                                                                          \
  }


namespace SD::Log::Engine {
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

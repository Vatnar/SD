#pragma once

#define ENGINE_LOG_LEVEL_TRACE 0
#define ENGINE_LOG_LEVEL_DEBUG 1
#define ENGINE_LOG_LEVEL_INFO 2
#define ENGINE_LOG_LEVEL_WARN 3
#define ENGINE_LOG_LEVEL_ERROR 4
#define ENGINE_LOG_LEVEL_CRITICAL 5
#define ENGINE_LOG_LEVEL_OFF 6

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

#ifndef ENGINE_LOG_LEVEL
#define ENGINE_LOG_LEVEL ENGINE_LOG_LEVEL_INFO
#endif

constexpr auto cLOG_LEVEL = static_cast<LogLevel>(ENGINE_LOG_LEVEL);

bool constexpr should_log(LogLevel level) {
  return static_cast<int>(level) >= static_cast<int>(cLOG_LEVEL);
}

/**
 * @brief Initializes asynchronous logging
 */
void Init();
} // namespace SD::Log

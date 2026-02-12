#pragma once

#define ENGINE_LOG_LEVEL_TRACE 0
#define ENGINE_LOG_LEVEL_DEBUG 1
#define ENGINE_LOG_LEVEL_INFO 2
#define ENGINE_LOG_LEVEL_WARN 3
#define ENGINE_LOG_LEVEL_ERROR 4
#define ENGINE_LOG_LEVEL_CRITICAL 5
#define ENGINE_LOG_LEVEL_OFF 6

#include <spdlog/async.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

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
inline void init_logging() {
  spdlog::init_thread_pool(8192, 1);

  std::vector<spdlog::sink_ptr> sinks;
  sinks.push_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());
  sinks.push_back(std::make_shared<spdlog::sinks::basic_file_sink_mt>("engine.log", true));

  auto engine = std::make_shared<spdlog::async_logger>("engine", sinks.begin(), sinks.end(),
                                                       spdlog::thread_pool(),
                                                       spdlog::async_overflow_policy::block);
  switch (cLOG_LEVEL) {
    case LogLevel::Trace:
      engine->set_level(spdlog::level::trace);
      break;
    case LogLevel::Debug:
      engine->set_level(spdlog::level::debug);
      break;
    case LogLevel::Info:
      engine->set_level(spdlog::level::info);
      break;
    case LogLevel::Warn:
      engine->set_level(spdlog::level::warn);
      break;
    case LogLevel::Error:
      engine->set_level(spdlog::level::err);
      break;
    case LogLevel::Critical:
      engine->set_level(spdlog::level::critical);
      break;
    case LogLevel::Off:
      engine->set_level(spdlog::level::off);
      break;
    default:
      engine->set_level(spdlog::level::info);
  }
  engine->flush_on(spdlog::level::warn);


  spdlog::register_logger(engine);
}

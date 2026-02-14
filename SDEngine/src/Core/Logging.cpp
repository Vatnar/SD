#include "Core/Logging.hpp"

#include "Core/Base.hpp"
#include "spdlog/async.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"
void init_logging() {
  spdlog::init_thread_pool(8192, 1);

  std::vector<spdlog::sink_ptr> sinks;
  sinks.push_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());
  sinks.push_back(std::make_shared<spdlog::sinks::basic_file_sink_mt>("engine.log", true));

  auto engine = std::make_shared<spdlog::async_logger>("engine", sinks.begin(), sinks.end(),
                                                       spdlog::thread_pool(),
                                                       spdlog::async_overflow_policy::block);
  SD_DISABLE_WARNING_PUSH
  SD_DISABLE_WARNING("-Wunreachable-code")
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
  SD_DISABLE_WARNING_POP

  engine->flush_on(spdlog::level::warn);


  spdlog::register_logger(engine);
}

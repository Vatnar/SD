#include "Core/Logging.hpp"

#include "spdlog/async.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/fmt/fmt.h"

#include <mutex>
#include "spdlog/sinks/base_sink.h"

namespace SD::Log {

static std::vector<LogEntry> sLogHistory;
static std::mutex sLogMutex;

const std::vector<LogEntry>& GetLogHistory() {
    return sLogHistory;
}

void AddLogEntry(LogEntry entry) {
    std::lock_guard<std::mutex> lock(sLogMutex);
    if (sLogHistory.size() > 500) sLogHistory.erase(sLogHistory.begin());
    sLogHistory.push_back(std::move(entry));
}

template<typename Mutex>
class imgui_sink : public spdlog::sinks::base_sink<Mutex> {
protected:
    void sink_it_(const spdlog::details::log_msg& msg) override {
        spdlog::memory_buf_t formatted;
        spdlog::sinks::base_sink<Mutex>::formatter_->format(msg, formatted);
        
        AddLogEntry({
            static_cast<LogLevel>(msg.level),
            fmt::to_string(formatted),
            "" // Timestamp is already in formatted string usually
        });
    }

    void flush_() override {}
};

using imgui_sink_mt = imgui_sink<std::mutex>;

void Init() {
  spdlog::init_thread_pool(8192, 1);

  std::vector<spdlog::sink_ptr> sinks;
  sinks.push_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());
  sinks.push_back(std::make_shared<spdlog::sinks::basic_file_sink_mt>("engine.log", true));
  sinks.push_back(std::make_shared<imgui_sink_mt>());

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
} // namespace SD::Log

#include "SD/core/logging.hpp"

#include <algorithm>
#include <chrono>
#include <mutex>
#include <vector>

#include "spdlog/async.h"
#include "spdlog/sinks/base_sink.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"

namespace sd::log {

static std::deque<LogEntry> g_log_history;
static std::mutex           g_log_mutex;

std::deque<LogEntry> get_log_history() {
  std::lock_guard lock(g_log_mutex);
  return g_log_history;
}

void add_log_entry(LogEntry entry) {
  std::lock_guard lock(g_log_mutex);
  if (g_log_history.size() > 500)
    g_log_history.erase(g_log_history.begin());
  g_log_history.push_back(std::move(entry));
}

static std::vector<CategoryInfo> g_category_registry;

std::vector<CategoryInfo>& get_category_registry() {
  return g_category_registry;
}

static std::vector<spdlog::sink_ptr> g_shared_sinks;
static spdlog::sink_ptr              g_engine_file_sink;
static spdlog::sink_ptr              g_game_file_sink;

spdlog::level::level_enum to_spdlog_level(const LogLevel level) {
  switch (level) {
    case LogLevel::TRACE:
      return spdlog::level::trace;
    case LogLevel::DEBUG:
      return spdlog::level::debug;
    case LogLevel::INFO:
      return spdlog::level::info;
    case LogLevel::WARN:
      return spdlog::level::warn;
    case LogLevel::ERROR:
      return spdlog::level::err;
    case LogLevel::CRITICAL:
      return spdlog::level::critical;
    case LogLevel::OFF:
      return spdlog::level::off;
  }
  return spdlog::level::info;
}

LogLevel from_spdlog_level(spdlog::level::level_enum level) {
  switch (level) {
    case spdlog::level::trace:
      return LogLevel::TRACE;
    case spdlog::level::debug:
      return LogLevel::DEBUG;
    case spdlog::level::info:
      return LogLevel::INFO;
    case spdlog::level::warn:
      return LogLevel::WARN;
    case spdlog::level::err:
      return LogLevel::ERROR;
    case spdlog::level::critical:
      return LogLevel::CRITICAL;
    case spdlog::level::off:
      return LogLevel::OFF;
    default:
      return LogLevel::INFO;
  }
}

static std::chrono::steady_clock::time_point s_start_time;

static float get_uptime_sec() {
  auto now     = std::chrono::steady_clock::now();
  auto elapsed = std::chrono::duration<float>(now - s_start_time).count();
  return elapsed;
}

template<typename Mutex>
class imgui_sink : public spdlog::sinks::base_sink<Mutex> {
protected:
  void sink_it_(const spdlog::details::log_msg& msg) override {
    spdlog::memory_buf_t formatted;
    spdlog::sinks::base_sink<Mutex>::formatter_->format(msg, formatted);

    add_log_entry({.category   = std::string(msg.logger_name.data(), msg.logger_name.size()),
                   .level      = from_spdlog_level(msg.level),
                   .message    = fmt::to_string(formatted),
                   .uptime_sec = get_uptime_sec()});
  }

  void flush_() override {}
};

using ImguiSinkMt = imgui_sink<std::mutex>;

static void create_category_logger(const char* name, const LogLevel minLevel) {
  if (g_shared_sinks.empty())
    return; // Init() not called yet

  std::vector<spdlog::sink_ptr> catSinks = g_shared_sinks; // console + imgui

  // Route to the correct file sink based on category prefix
  std::string_view sv(name);
  if (sv.starts_with("game")) {
    catSinks.push_back(g_game_file_sink);
  } else {
    // Unknown category -> engine.log as fallback
    catSinks.push_back(g_engine_file_sink);
  }

  auto logger = std::make_shared<spdlog::async_logger>(name, catSinks.begin(), catSinks.end(),
                                                       spdlog::thread_pool(),
                                                       spdlog::async_overflow_policy::block);

  logger->set_level(to_spdlog_level(minLevel));
  logger->flush_on(spdlog::level::err); // Immediate flush on error/critical for crash debugging
  spdlog::register_logger(logger);
}

void register_category(const char* name, ImVec4 color) {
  // Avoid duplicates
  auto it = std::find_if(g_category_registry.begin(), g_category_registry.end(),
                         [name](const CategoryInfo& ci) { return ci.name == name; });
  if (it == g_category_registry.end()) {
    g_category_registry.push_back(CategoryInfo{.name = name, .visible = true, .color = color});
    // Create the logger immediately
    create_category_logger(name, LogLevel::TRACE);
  }
}

bool is_category_under(const std::string& child, const std::string& parent) {
  if (child == parent)
    return true;
  if (child.size() <= parent.size())
    return false;
  // child starts with parent + "/"
  return child.compare(0, parent.size(), parent) == 0 && child[parent.size()] == '/';
}

void init() {
  s_start_time = std::chrono::steady_clock::now();
  g_category_registry.clear();
  g_shared_sinks.clear();

  spdlog::init_thread_pool(8192, 1);

  // Shared sinks
  auto console = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
  auto imgui   = std::make_shared<ImguiSinkMt>();

  auto engine_file = std::make_shared<spdlog::sinks::basic_file_sink_mt>("engine.log", true);
  auto game_file   = std::make_shared<spdlog::sinks::basic_file_sink_mt>("game.log", true);

  // Console & file: [HH:MM:SS.mmm] [category] [level] message
  std::string pattern = "%H:%M:%S.%e [%n] [%^%l%$] %v";
  console->set_pattern(pattern);
  engine_file->set_pattern(pattern);
  game_file->set_pattern(pattern);

  // ImGui sink: raw message only (timestamp/category/level added by ImGui display code)
  imgui->set_pattern("%v");

  g_shared_sinks     = {console, imgui};
  g_engine_file_sink = engine_file;
  g_game_file_sink   = game_file;

  register_category("engine", ImVec4(0.0f, 0.8f, 1.0f, 1.0f));          // Cyan
  register_category("engine/renderer", ImVec4(0.8f, 0.4f, 1.0f, 1.0f)); // Purple
  register_category("engine/ecs", ImVec4(1.0f, 0.8f, 0.0f, 1.0f));      // Yellow
  register_category("engine/network", ImVec4(0.0f, 1.0f, 0.6f, 1.0f));  // Teal
}

} // namespace sd::log

namespace sd::log::game {
SD_LOG_CATEGORY_IMPL("game", ENGINE_LOG_LEVEL_GAME)

namespace combat {
SD_LOG_CATEGORY_IMPL("game/combat", ENGINE_LOG_LEVEL_GAME)
}
} // namespace sd::log::game

#include "SD/core/logging.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <mutex>
#include <vector>

#include <fmt/chrono.h>
#include <fmt/format.h>
#include <quill/Backend.h>
#include <quill/Frontend.h>
#include <quill/sinks/FileSink.h>
#include <quill/sinks/Sink.h>

#include "SD/profiler.hpp"

namespace sd::log {

FILE_INTERNAL_BEGIN

constexpr const char* HISTORY_FILE    = "debug_history.log";
constexpr size_t      MAX_MEM_ENTRIES = 500;


std::deque<LogEntry> g_log_history;
size_t               g_file_entry_count = 0;
std::mutex           g_log_mutex;

void write_entry_to_file(const LogEntry& entry) {
  FILE* f = fopen(HISTORY_FILE, "a");
  if (!f)
    return;
  fprintf(f,
          "%.6f|%d|%s|%s\n",
          entry.uptime_sec,
          static_cast<int>(entry.level),
          entry.category.c_str(),
          entry.message.c_str());
  fclose(f);
}

std::vector<LogEntry> read_entries_from_file(size_t offset, size_t count) {
  std::vector<LogEntry> result;
  FILE*                 f = fopen(HISTORY_FILE, "r");
  if (!f)
    return result;

  char   line[4096];
  size_t idx = 0;
  while (fgets(line, sizeof(line), f) && result.size() < count) {
    if (idx++ < offset)
      continue;

    char* p1 = strchr(line, '|');
    if (!p1)
      continue;
    *p1++ = '\0';

    char* p2 = strchr(p1, '|');
    if (!p2)
      continue;
    *p2++ = '\0';

    char* p3 = strchr(p2, '|');
    if (!p3)
      continue;
    *p3++ = '\0';

    size_t mlen = strlen(p3);
    while (mlen > 0 && (p3[mlen - 1] == '\n' || p3[mlen - 1] == '\r'))
      p3[--mlen] = '\0';

    LogEntry entry;
    entry.uptime_sec = std::stof(line);
    entry.level      = static_cast<LogLevel>(std::stoi(p1));
    entry.category   = p2;
    entry.message    = p3;
    result.push_back(std::move(entry));
  }

  fclose(f);
  return result;
}

FILE_INTERNAL_END

std::deque<LogEntry> get_log_history() {
  std::lock_guard lock(FILE_INTERNAL::g_log_mutex);
  return FILE_INTERNAL::g_log_history;
}

void add_log_entry(LogEntry entry) {
  std::lock_guard lock(FILE_INTERNAL::g_log_mutex);
  if (FILE_INTERNAL::g_log_history.size() >= FILE_INTERNAL::MAX_MEM_ENTRIES) {
    FILE_INTERNAL::write_entry_to_file(FILE_INTERNAL::g_log_history.front());
    FILE_INTERNAL::g_log_history.pop_front();
    FILE_INTERNAL::g_file_entry_count++;
  }
  FILE_INTERNAL::g_log_history.push_back(std::move(entry));
}

void clear_history() {
  std::lock_guard lock(FILE_INTERNAL::g_log_mutex);
  FILE_INTERNAL::g_log_history.clear();
  FILE_INTERNAL::g_file_entry_count = 0;
  FILE* f                           = fopen(FILE_INTERNAL::HISTORY_FILE, "w");
  if (f)
    fclose(f);
}

size_t get_total_entry_count() {
  std::lock_guard lock(FILE_INTERNAL::g_log_mutex);
  return FILE_INTERNAL::g_file_entry_count + FILE_INTERNAL::g_log_history.size();
}

std::vector<LogEntry> get_entries(size_t offset, size_t count) {
  std::lock_guard       lock(FILE_INTERNAL::g_log_mutex);
  std::vector<LogEntry> result;

  if (offset < FILE_INTERNAL::g_file_entry_count) {
    size_t from_file = std::min(count, FILE_INTERNAL::g_file_entry_count - offset);
    auto   fe        = FILE_INTERNAL::read_entries_from_file(offset, from_file);
    result           = std::move(fe);
  }

  size_t mem_offset =
      offset > FILE_INTERNAL::g_file_entry_count ? offset - FILE_INTERNAL::g_file_entry_count : 0;
  if (mem_offset < FILE_INTERNAL::g_log_history.size() && result.size() < count) {
    size_t from_mem =
        std::min(count - result.size(), FILE_INTERNAL::g_log_history.size() - mem_offset);
    auto begin = FILE_INTERNAL::g_log_history.begin() + static_cast<std::ptrdiff_t>(mem_offset);
    result.insert(result.end(), begin, begin + static_cast<std::ptrdiff_t>(from_mem));
  }

  return result;
}

FILE_INTERNAL_BEGIN

std::vector<CategoryInfo> g_category_registry;
std::mutex                g_registry_mutex;

FILE_INTERNAL_END

std::vector<CategoryInfo>& get_category_registry() {
  return FILE_INTERNAL::g_category_registry;
}

quill::LogLevel to_quill_level(const LogLevel level) {
  switch (level) {
    case LogLevel::TRACE:
      return quill::LogLevel::TraceL3;
    case LogLevel::DEBUG:
      return quill::LogLevel::Debug;
    case LogLevel::INFO:
      return quill::LogLevel::Info;
    case LogLevel::WARN:
      return quill::LogLevel::Warning;
    case LogLevel::ERROR:
      return quill::LogLevel::Error;
    case LogLevel::CRITICAL:
      return quill::LogLevel::Critical;
    case LogLevel::OFF:
      return quill::LogLevel::None;
    default:
      return quill::LogLevel::Info;
  }
}

LogLevel from_quill_level(const quill::LogLevel level) {
  switch (level) {
    case quill::LogLevel::TraceL3:
    case quill::LogLevel::TraceL2:
    case quill::LogLevel::TraceL1:
      return LogLevel::TRACE;
    case quill::LogLevel::Debug:
      return LogLevel::DEBUG;
    case quill::LogLevel::Info:
    case quill::LogLevel::Notice:
      return LogLevel::INFO;
    case quill::LogLevel::Warning:
      return LogLevel::WARN;
    case quill::LogLevel::Error:
      return LogLevel::ERROR;
    case quill::LogLevel::Critical:
      return LogLevel::CRITICAL;
    case quill::LogLevel::None:
      return LogLevel::OFF;
    default:
      return LogLevel::GENERAL;
  }
}

FILE_INTERNAL_BEGIN

std::chrono::steady_clock::time_point s_start_time;

float get_uptime_sec() {
  auto now     = std::chrono::steady_clock::now();
  auto elapsed = std::chrono::duration<float>(now - s_start_time).count();
  return elapsed;
}

std::string ansi_fg_color(ImVec4 c) {
  return fmt::format("\033[38;2;{};{};{}m",
                     static_cast<int>(c.x * 255),
                     static_cast<int>(c.y * 255),
                     static_cast<int>(c.z * 255));
}

std::string level_ansi_color(quill::LogLevel level) {
  ImVec4 c;
  switch (level) {
    case quill::LogLevel::TraceL3:
    case quill::LogLevel::TraceL2:
    case quill::LogLevel::TraceL1:
      c = ImVec4{0.7f, 0.7f, 0.7f, 1.0f};
      break;
    case quill::LogLevel::Debug:
      c = ImVec4{0.0f, 0.8f, 1.0f, 1.0f};
      break;
    case quill::LogLevel::Info:
    case quill::LogLevel::Notice:
      c = ImVec4{0.0f, 1.0f, 0.0f, 1.0f};
      break;
    case quill::LogLevel::Warning:
      c = ImVec4{1.0f, 1.0f, 0.0f, 1.0f};
      break;
    case quill::LogLevel::Error:
      c = ImVec4{1.0f, 0.0f, 0.0f, 1.0f};
      break;
    case quill::LogLevel::Critical:
      c = ImVec4{1.0f, 0.0f, 1.0f, 1.0f};
      break;
    default:
      c = ImVec4{0.7f, 0.7f, 0.7f, 1.0f};
      break;
  }
  return ansi_fg_color(c);
}

const char* level_str(quill::LogLevel level) {
  switch (level) {
    case quill::LogLevel::TraceL3:
    case quill::LogLevel::TraceL2:
    case quill::LogLevel::TraceL1:
      return "trace";
    case quill::LogLevel::Debug:
      return "debug";
    case quill::LogLevel::Info:
    case quill::LogLevel::Notice:
      return "info";
    case quill::LogLevel::Warning:
      return "warn";
    case quill::LogLevel::Error:
      return "error";
    case quill::LogLevel::Critical:
      return "critical";
    default:
      return "?";
  }
}

FILE_INTERNAL_END

class CategoryConsoleSink final : public quill::Sink {
public:
  CategoryConsoleSink() :
    quill::Sink(quill::PatternFormatterOptions{
        "%(time) ",      // format_pattern: just the timestamp prefix
        "%H:%M:%S.%Qus", // timestamp_pattern: HH:MM:SS.ffffff
        quill::Timezone::LocalTime,
        true,
        '\0' // no suffix — we add our own
    }) {}

  void write_log(const quill::MacroMetadata*,
                 uint64_t,
                 std::string_view,
                 std::string_view,
                 const std::string&,
                 std::string_view logger_name,
                 quill::LogLevel  log_level,
                 std::string_view,
                 std::string_view,
                 const std::vector<std::pair<std::string, std::string>>*,
                 std::string_view log_message,
                 std::string_view log_statement) override {
    std::string cat_color;
    {
      std::lock_guard lock(FILE_INTERNAL::g_registry_mutex);
      auto&           reg = get_category_registry();
      for (auto& cat : reg) {
        if (is_category_under(std::string(logger_name.data(), logger_name.size()), cat.name)) {
          cat_color = FILE_INTERNAL::ansi_fg_color(cat.color);
          break;
        }
      }
    }

    std::string out;
    out.append(log_statement.data(), log_statement.size());

    if (!cat_color.empty()) {
      out += cat_color;
      out += '[';
      out.append(logger_name.data(), logger_name.size());
      out += ']';
      out += "\033[0m";
    } else {
      out += '[';
      out.append(logger_name.data(), logger_name.size());
      out += ']';
    }

    // tagged/general messages start with '[' — no level label
    if (log_message.size() > 0 && log_message[0] == '[') {
      out += ' ';
      auto close = log_message.find(']');
      if (close != std::string_view::npos && !cat_color.empty()) {
        out += cat_color;
        out.append(log_message.data(), close + 1);
        out += "\033[0m";
        out.append(log_message.data() + close + 1, log_message.size() - close - 1);
      } else {
        out.append(log_message.data(), log_message.size());
      }
    } else {
      out += " [";
      out += FILE_INTERNAL::level_ansi_color(log_level);
      out += FILE_INTERNAL::level_str(log_level);
      out += "\033[0m] ";
      out.append(log_message.data(), log_message.size());
    }

    out += '\n';
    fwrite(out.data(), 1, out.size(), stdout);
  }

  void flush_sink() noexcept override { fflush(stdout); }
};

class ImguiSink final : public quill::Sink {
public:
  ImguiSink() :
    quill::Sink(quill::PatternFormatterOptions{"%v", // message only
                                               "%H:%M:%S.%Qus",
                                               quill::Timezone::LocalTime,
                                               true,
                                               quill::PatternFormatterOptions::NO_SUFFIX}) {}

  void write_log(const quill::MacroMetadata*,
                 uint64_t,
                 std::string_view,
                 std::string_view,
                 const std::string&,
                 std::string_view logger_name,
                 quill::LogLevel  log_level,
                 std::string_view,
                 std::string_view,
                 const std::vector<std::pair<std::string, std::string>>*,
                 std::string_view log_message,
                 std::string_view) override {
    add_log_entry({.category   = std::string(logger_name.data(), logger_name.size()),
                   .level      = from_quill_level(log_level),
                   .message    = std::string(log_message.data(), log_message.size()),
                   .uptime_sec = FILE_INTERNAL::get_uptime_sec()});
  }

  void flush_sink() noexcept override {}
};

FILE_INTERNAL_BEGIN

std::shared_ptr<quill::Sink> g_console_sink;
std::shared_ptr<quill::Sink> g_imgui_sink;
std::shared_ptr<quill::Sink> g_engine_file_sink;
std::shared_ptr<quill::Sink> g_game_file_sink;
std::shared_ptr<quill::Sink> g_profiler_file_sink;

void create_category_logger(const char* name, LogLevel minLevel) {
  if (!g_console_sink)
    return;

  if (quill::Frontend::get_logger(name))
    return;

  std::vector<std::shared_ptr<quill::Sink>> catSinks = {g_console_sink, g_imgui_sink};

  std::string_view sv(name);
  if (sv == "profiler") {
    catSinks.push_back(g_profiler_file_sink);
  } else if (sv.starts_with("game")) {
    catSinks.push_back(g_game_file_sink);
  } else {
    catSinks.push_back(g_engine_file_sink);
  }

  auto* logger = quill::Frontend::create_or_get_logger(name, std::move(catSinks));
  logger->set_log_level(to_quill_level(minLevel));
  logger->set_immediate_flush(0);
}

FILE_INTERNAL_END

void register_category(const char* name, ImVec4 color) {
  bool found = false;
  {
    std::lock_guard lock(FILE_INTERNAL::g_registry_mutex);
    auto            it = std::find_if(FILE_INTERNAL::g_category_registry.begin(),
                                      FILE_INTERNAL::g_category_registry.end(),
                                      [name](const CategoryInfo& ci) { return ci.name == name; });
    if (it == FILE_INTERNAL::g_category_registry.end()) {
      FILE_INTERNAL::g_category_registry.push_back(
          CategoryInfo{.name = name, .visible = true, .color = color});
    } else {
      found = true;
    }
  }
  if (!found) {
    FILE_INTERNAL::create_category_logger(name, LogLevel::TRACE);
  }
}

bool is_category_under(const std::string& child, const std::string& parent) {
  if (child == parent)
    return true;
  if (child.size() <= parent.size())
    return false;
  return child.compare(0, parent.size(), parent) == 0 && child[parent.size()] == '/';
}

void init() {
  FILE_INTERNAL::s_start_time = std::chrono::steady_clock::now();
  FILE_INTERNAL::g_category_registry.clear();
  FILE_INTERNAL::g_console_sink.reset();
  FILE_INTERNAL::g_imgui_sink.reset();
  FILE_INTERNAL::g_engine_file_sink.reset();
  FILE_INTERNAL::g_game_file_sink.reset();
  FILE_INTERNAL::g_profiler_file_sink.reset();

  std::rename(FILE_INTERNAL::HISTORY_FILE, "debug_history.log.old");

  quill::BackendOptions backend_opts;
  backend_opts.error_notifier = [](const std::string&) {
  };
  quill::Backend::start(backend_opts);

  FILE_INTERNAL::g_console_sink =
      quill::Frontend::create_or_get_sink<CategoryConsoleSink>("console");
  FILE_INTERNAL::g_imgui_sink = quill::Frontend::create_or_get_sink<ImguiSink>("imgui");

  auto make_file_sink = [](const char* filename) {
    quill::FileSinkConfig cfg;
    cfg.set_open_mode('w');
    quill::PatternFormatterOptions file_pattern(
        "%(time) [%(logger)] %(log_level_short_code) %(message)",
        "%H:%M:%S.%Qms",
        quill::Timezone::LocalTime,
        true,
        '\n');
    cfg.set_override_pattern_formatter_options(std::move(file_pattern));
    return quill::Frontend::create_or_get_sink<quill::FileSink>(std::filesystem::path{filename},
                                                                std::move(cfg));
  };

  FILE_INTERNAL::g_engine_file_sink   = make_file_sink("engine.log");
  FILE_INTERNAL::g_game_file_sink     = make_file_sink("game.log");
  FILE_INTERNAL::g_profiler_file_sink = make_file_sink("profiler.log");

  register_category("engine", ImVec4(0.0f, 0.8f, 1.0f, 1.0f));
  register_category("engine/renderer", ImVec4(0.8f, 0.4f, 1.0f, 1.0f));
  register_category("profiler", ImVec4(1.0f, 0.4f, 1.0f, 1.0f));
  register_category("engine/ecs", ImVec4(1.0f, 0.8f, 0.0f, 1.0f));
  register_category("engine/network", ImVec4(0.0f, 1.0f, 0.6f, 1.0f));
  register_category("engine/shader", ImVec4(1.0f, 0.8f, 0.0f, 1.0f));
  register_category("vulkan", ImVec4(1.0f, 0.4f, 0.0f, 1.0f));
  register_category("debug_layer", ImVec4(1.0f, 0.5f, 0.8f, 1.0f));

  sd::detail::get_cpu_mhz();
}

SD_EXPORT quill::Logger* get_category_logger_or_report(const char* categoryPath) {
  auto* logger = quill::Frontend::get_logger(categoryPath);
  if (!logger) {
    fmt::println(stderr,
                 "Log category '{}' is not registered. Message was not logged.",
                 categoryPath);
  }
  return logger;
}

} // namespace sd::log

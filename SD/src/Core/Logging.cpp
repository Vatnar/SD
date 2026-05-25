#include "Core/Logging.hpp"

#include "spdlog/async.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/sinks/base_sink.h"

#include <algorithm>
#include <chrono>
#include <mutex>
#include <vector>

namespace SD::Log {

// ============================================================================
// Log history
// ============================================================================
static std::deque<LogEntry> sLogHistory;
static std::mutex sLogMutex;

const std::deque<LogEntry>& GetLogHistory() {
    return sLogHistory;
}

void AddLogEntry(LogEntry entry) {
    std::lock_guard<std::mutex> lock(sLogMutex);
    if (sLogHistory.size() > 500) sLogHistory.erase(sLogHistory.begin());
    sLogHistory.push_back(std::move(entry));
}

// ============================================================================
// Category registry
// ============================================================================
static std::vector<CategoryInfo> sCategoryRegistry;

std::vector<CategoryInfo>& GetCategoryRegistry() {
    return sCategoryRegistry;
}

// ============================================================================
// Shared sinks & file sinks (stored after Init so RegisterCategory can use them)
// ============================================================================
static std::vector<spdlog::sink_ptr> sSharedSinks;
static spdlog::sink_ptr sEngineFileSink;
static spdlog::sink_ptr sGameFileSink;

// ============================================================================
// Level conversions
// ============================================================================
spdlog::level::level_enum ToSpdlogLevel(LogLevel level) {
    switch (level) {
        case LogLevel::Trace:    return spdlog::level::trace;
        case LogLevel::Debug:    return spdlog::level::debug;
        case LogLevel::Info:     return spdlog::level::info;
        case LogLevel::Warn:     return spdlog::level::warn;
        case LogLevel::Error:    return spdlog::level::err;
        case LogLevel::Critical: return spdlog::level::critical;
        case LogLevel::Off:      return spdlog::level::off;
    }
    return spdlog::level::info;
}

LogLevel FromSpdlogLevel(spdlog::level::level_enum level) {
    switch (level) {
        case spdlog::level::trace:    return LogLevel::Trace;
        case spdlog::level::debug:    return LogLevel::Debug;
        case spdlog::level::info:     return LogLevel::Info;
        case spdlog::level::warn:     return LogLevel::Warn;
        case spdlog::level::err:      return LogLevel::Error;
        case spdlog::level::critical: return LogLevel::Critical;
        case spdlog::level::off:      return LogLevel::Off;
        default:                      return LogLevel::Info;
    }
}

// ============================================================================
// Uptime tracking
// ============================================================================
static std::chrono::steady_clock::time_point sStartTime;

static float GetUptimeSec() {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration<float>(now - sStartTime).count();
    return elapsed;
}

// ============================================================================
// ImGui sink — feeds the in-game log viewer
// ============================================================================
template<typename Mutex>
class imgui_sink : public spdlog::sinks::base_sink<Mutex> {
protected:
    void sink_it_(const spdlog::details::log_msg& msg) override {
        spdlog::memory_buf_t formatted;
        spdlog::sinks::base_sink<Mutex>::formatter_->format(msg, formatted);

        AddLogEntry({
            .category = std::string(msg.logger_name.data(), msg.logger_name.size()),
            .level = FromSpdlogLevel(msg.level),
            .message = fmt::to_string(formatted),
            .uptimeSec = GetUptimeSec()
        });
    }

    void flush_() override {}
};

using imgui_sink_mt = imgui_sink<std::mutex>;

// ============================================================================
// Helper: create a logger for a category with the right sinks
// ============================================================================
static void CreateCategoryLogger(
    const char* name,
    LogLevel minLevel)
{
    if (sSharedSinks.empty()) return;  // Init() not called yet

    std::vector<spdlog::sink_ptr> catSinks = sSharedSinks;  // console + imgui

    // Route to the correct file sink based on category prefix
    std::string_view sv(name);
    if (sv.starts_with("Engine")) {
        catSinks.push_back(sEngineFileSink);
    } else if (sv.starts_with("Game")) {
        catSinks.push_back(sGameFileSink);
    } else {
        // Unknown category -> engine.log as fallback
        catSinks.push_back(sEngineFileSink);
    }

    auto logger = std::make_shared<spdlog::async_logger>(
        name, catSinks.begin(), catSinks.end(),
        spdlog::thread_pool(),
        spdlog::async_overflow_policy::block);

    logger->set_level(ToSpdlogLevel(minLevel));
    logger->flush_on(spdlog::level::err);  // Immediate flush on error/critical for crash debugging
    spdlog::register_logger(logger);
}

// ============================================================================
// Public: Register a category at runtime (for game modules or plugins)
// ============================================================================
void RegisterCategory(const char* name, ImVec4 color) {
    // Avoid duplicates
    auto it = std::find_if(sCategoryRegistry.begin(), sCategoryRegistry.end(),
                             [name](const CategoryInfo& ci) { return ci.name == name; });
    if (it == sCategoryRegistry.end()) {
        sCategoryRegistry.push_back(CategoryInfo{ .name = name, .visible = true, .color = color });
        // Create the logger immediately
        CreateCategoryLogger(name, LogLevel::Info);
    }
}

bool IsCategoryUnder(const std::string& child, const std::string& parent) {
    if (child == parent) return true;
    if (child.size() <= parent.size()) return false;
    // child starts with parent + "/"
    return child.compare(0, parent.size(), parent) == 0 && child[parent.size()] == '/';
}

// ============================================================================
// Init
// ============================================================================
void Init() {
    sStartTime = std::chrono::steady_clock::now();
    sCategoryRegistry.clear();
    sSharedSinks.clear();

    spdlog::init_thread_pool(8192, 1);

    // Shared sinks
    auto console = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    auto imgui   = std::make_shared<imgui_sink_mt>();

    // File sinks — one per tree
    auto engineFile = std::make_shared<spdlog::sinks::basic_file_sink_mt>("engine.log", true);
    auto gameFile   = std::make_shared<spdlog::sinks::basic_file_sink_mt>("game.log", true);

    // Console & file: [HH:MM:SS.mmm] [category] [level] message
    std::string pattern = "%H:%M:%S.%e [%n] [%^%l%$] %v";
    console->set_pattern(pattern);
    engineFile->set_pattern(pattern);
    gameFile->set_pattern(pattern);

    // ImGui sink: raw message only (timestamp/category/level added by ImGui display code)
    imgui->set_pattern("%v");

    sSharedSinks = { console, imgui };
    sEngineFileSink = engineFile;
    sGameFileSink   = gameFile;

    // ------------------------------------------------------------------------
    // Register engine categories with colors and create their loggers
    // ------------------------------------------------------------------------
    RegisterCategory("Engine",          ImVec4(0.0f, 0.8f, 1.0f, 1.0f));   // Cyan
    RegisterCategory("Engine/Renderer", ImVec4(0.8f, 0.4f, 1.0f, 1.0f));   // Purple
    RegisterCategory("Engine/ECS",      ImVec4(1.0f, 0.8f, 0.0f, 1.0f));   // Yellow
    RegisterCategory("Engine/Network",  ImVec4(0.0f, 1.0f, 0.6f, 1.0f));   // Teal
}

} // namespace SD::Log

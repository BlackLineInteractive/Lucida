#include "lucida/core/diag/Log.h"
#include "lucida/core/diag/Assert.h"

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <mutex>

namespace lucida {
namespace {

constexpr const char* kChannelNames[] = {
    "core", "memory", "runtime", "render", "physics", "resource", "input", "framework", "app"
};
static_assert(sizeof(kChannelNames) / sizeof(kChannelNames[0])
              == static_cast<usize>(LogChannel::Count));

constexpr const char* kLevelStyle[] = {
    "\x1b[90m", "\x1b[36m", "\x1b[32m", "\x1b[33m", "\x1b[31m"
};
constexpr const char* kLevelTag[] = { "trace", "debug", "info ", "warn ", "error" };

struct LogState {
    LogLevel   global = LogLevel::Info;
    LogLevel   channel[static_cast<usize>(LogChannel::Count)];
    std::mutex mutex;

    LogState() {
        for (auto& lvl : channel) lvl = LogLevel::Trace;  // channel does not filter until asked
    }
};

LogState& State() {
    static LogState s;
    return s;
}

} // namespace

void LogSetLevel(LogLevel level) { State().global = level; }

void LogSetChannelLevel(LogChannel channel, LogLevel level) {
    State().channel[static_cast<usize>(channel)] = level;
}

bool LogEnabled(LogChannel channel, LogLevel level) {
    const LogState& s = State();
    const LogLevel chLevel = s.channel[static_cast<usize>(channel)];
    const LogLevel effective = (chLevel > s.global) ? chLevel : s.global;
    return level >= effective && effective != LogLevel::Off;
}

void LogWrite(LogChannel channel, LogLevel level, const char* fmt, ...) {
    const usize lvl = static_cast<usize>(level);
    if (lvl >= static_cast<usize>(LogLevel::Off)) return;

    char message[2048];
    va_list args;
    va_start(args, fmt);
    std::vsnprintf(message, sizeof(message), fmt, args);
    va_end(args);

    // One locked fprintf per record, or threads interleave inside a line.
    std::lock_guard<std::mutex> lock(State().mutex);
    std::fprintf(stderr, "%s[%s]\x1b[0m \x1b[1m%-9s\x1b[0m %s\n",
                 kLevelStyle[lvl], kLevelTag[lvl],
                 kChannelNames[static_cast<usize>(channel)], message);
}

namespace diag {

void ReportAssertFailure(const char* expr, const char* file, int line, const char* fmt, ...) {
    char message[1024] = {0};
    if (fmt && fmt[0]) {
        va_list args;
        va_start(args, fmt);
        std::vsnprintf(message, sizeof(message), fmt, args);
        va_end(args);
    }
    std::fprintf(stderr,
                 "\x1b[41;97m ASSERT \x1b[0m \x1b[1m%s\x1b[0m\n"
                 "         %s:%d\n"
                 "         %s\n",
                 expr, file, line, message);
    std::fflush(stderr);
}

} // namespace diag
} // namespace lucida

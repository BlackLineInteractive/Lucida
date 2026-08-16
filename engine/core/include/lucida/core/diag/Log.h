// Lucida Engine
// Copyright (C) 2026 BlackLine Interactive
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
// Channelled logging (GEA 3.5). printf-style: no <iostream>, no per-call allocation.

#include "lucida/core/platform/Platform.h"

namespace lucida {

enum class LogLevel : u8 { Trace = 0, Debug, Info, Warn, Error, Off };

// Adding a channel: extend this enum and kChannelNames in Log.cpp.
enum class LogChannel : u8 {
    Core = 0, Memory, Runtime, Render, Physics, Resource, Input, Framework, App,
    Count
};

void LogSetLevel(LogLevel level);
void LogSetChannelLevel(LogChannel channel, LogLevel level);
bool LogEnabled(LogChannel channel, LogLevel level);
void LogWrite(LogChannel channel, LogLevel level, const char* fmt, ...);

using LogSinkFn = void(*)(LogChannel channel, LogLevel level, const char* message, void* user_data);
void LogAddSink(LogSinkFn sink, void* user_data = nullptr);
void LogRemoveSink(LogSinkFn sink);

} // namespace lucida

#define LUCIDA_LOG(ch, lvl, ...)                                              \
    do {                                                                      \
        if (::lucida::LogEnabled(::lucida::LogChannel::ch, ::lucida::LogLevel::lvl)) \
            ::lucida::LogWrite(::lucida::LogChannel::ch, ::lucida::LogLevel::lvl, __VA_ARGS__); \
    } while (0)

#define LUCIDA_TRACE(ch, ...) LUCIDA_LOG(ch, Trace, __VA_ARGS__)
#define LUCIDA_DEBUG(ch, ...) LUCIDA_LOG(ch, Debug, __VA_ARGS__)
#define LUCIDA_INFO(ch, ...)  LUCIDA_LOG(ch, Info,  __VA_ARGS__)
#define LUCIDA_WARN(ch, ...)  LUCIDA_LOG(ch, Warn,  __VA_ARGS__)
#define LUCIDA_ERROR(ch, ...) LUCIDA_LOG(ch, Error, __VA_ARGS__)

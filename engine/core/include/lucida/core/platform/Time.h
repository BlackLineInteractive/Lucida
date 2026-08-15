// Lucida Engine
// Copyright (C) 2026 BlackLine Interactive
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
// Time source (GEA 8.5). One monotonic clock for the whole engine.

#include "lucida/core/platform/Platform.h"

#include <chrono>

namespace lucida {

class Clock {
public:
    using Native = std::chrono::steady_clock;
    using Point  = Native::time_point;

    static Point Now() { return Native::now(); }

    static f64 SecondsBetween(Point a, Point b) {
        return std::chrono::duration<f64>(b - a).count();
    }
    static f64 MillisBetween(Point a, Point b) {
        return std::chrono::duration<f64, std::milli>(b - a).count();
    }
};

class Stopwatch {
public:
    Stopwatch() : m_start(Clock::Now()) {}

    void Restart()              { m_start = Clock::Now(); }
    f64  ElapsedSeconds() const { return Clock::SecondsBetween(m_start, Clock::Now()); }
    f64  ElapsedMillis()  const { return Clock::MillisBetween(m_start, Clock::Now()); }

private:
    Clock::Point m_start;
};

// Per-frame timing handed to systems.
// delta is the fixed simulation step; real_delta is wall time, for stats only.
struct FrameTime {
    f32 delta       = 0.0f;
    f32 real_delta  = 0.0f;
    f32 alpha       = 0.0f;   // 0..1 leftover, for render interpolation
    f64 elapsed     = 0.0;
    u64 frame_index = 0;
    u32 tick_count  = 0;      // simulation steps taken this frame
};

} // namespace lucida

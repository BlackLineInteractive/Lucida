// Lucida Engine
// Copyright (C) 2026 BlackLine Interactive
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
// Game Loop (GPP ch.3).
//
// "Play catch up": simulation advances in fixed steps, rendering runs as often
// as the machine allows and interpolates with the leftover. Physics with a
// variable step is not deterministic and blows up at low frame rates, so the
// step is fixed and only the number of steps per frame varies.

#include "lucida/core/platform/Time.h"

namespace lucida {

struct LoopConfig {
    f32 fixed_step     = 1.0f / 60.0f;  // simulation step, seconds
    u32 max_ticks      = 5;             // catch-up cap, see below
    f32 max_frame_time = 0.25f;         // clamp after a stall (debugger, loading)
};

class GameLoop {
public:
    void Init(const LoopConfig& config) {
        m_config = config;
        m_prev   = Clock::Now();
        m_start  = m_prev;
        m_lag    = 0.0f;
        m_time   = FrameTime{};
    }

    // Call once at the top of the frame.
    const FrameTime& BeginFrame() {
        const Clock::Point now = Clock::Now();
        f32 elapsed = static_cast<f32>(Clock::SecondsBetween(m_prev, now));
        m_prev = now;

        // Without the clamp a paused debugger produces one frame with a
        // multi-second delta, and the catch-up loop spirals.
        if (elapsed > m_config.max_frame_time) elapsed = m_config.max_frame_time;

        m_lag += elapsed;

        m_time.real_delta = elapsed;
        m_time.delta      = m_config.fixed_step;
        m_time.elapsed    = Clock::SecondsBetween(m_start, now);
        m_time.tick_count = 0;
        ++m_time.frame_index;
        return m_time;
    }

    // while (loop.StepSimulation()) { world.FixedUpdate(loop.Time()); }
    bool StepSimulation() {
        if (m_lag < m_config.fixed_step) return false;
        if (m_time.tick_count >= m_config.max_ticks) {
            // Simulation cannot keep up. Drop the backlog rather than fall
            // further behind every frame.
            m_lag = 0.0f;
            return false;
        }
        m_lag -= m_config.fixed_step;
        ++m_time.tick_count;
        return true;
    }

    // Call after stepping; alpha is what the renderer interpolates with.
    const FrameTime& EndSimulation() {
        m_time.alpha = m_lag / m_config.fixed_step;
        return m_time;
    }

    const FrameTime& Time()   const { return m_time; }
    const LoopConfig& Config() const { return m_config; }
    void SetFixedStep(f32 step) { m_config.fixed_step = step; }

private:
    LoopConfig   m_config;
    FrameTime    m_time;
    Clock::Point m_prev{};
    Clock::Point m_start{};
    f32          m_lag = 0.0f;
};

} // namespace lucida

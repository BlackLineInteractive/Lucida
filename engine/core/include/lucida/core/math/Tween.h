// Lucida Engine
// Copyright (C) 2026 BlackLine Interactive
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "lucida/core/math/Math.h"
#include <cmath>

namespace lucida {

enum class EaseType : u8 {
    Linear,
    InQuad,
    OutQuad,
    InOutQuad,
    InCubic,
    OutCubic,
    InOutCubic,
    InExpo,
    OutExpo,
    InOutExpo,
    InBounce,
    OutBounce,
    InOutBounce,
    OutElastic
};

inline f32 Ease(EaseType type, f32 t) {
    t = Clamp(t, 0.0f, 1.0f);
    switch (type) {
    case EaseType::Linear:
        return t;
    case EaseType::InQuad:
        return t * t;
    case EaseType::OutQuad:
        return t * (2.0f - t);
    case EaseType::InOutQuad:
        return t < 0.5f ? 2.0f * t * t : -1.0f + (4.0f - 2.0f * t) * t;
    case EaseType::InCubic:
        return t * t * t;
    case EaseType::OutCubic: {
        f32 f = t - 1.0f;
        return f * f * f + 1.0f;
    }
    case EaseType::InOutCubic:
        return t < 0.5f ? 4.0f * t * t * t : (t - 1.0f) * (2.0f * t - 2.0f) * (2.0f * t - 2.0f) + 1.0f;
    case EaseType::InExpo:
        return (t == 0.0f) ? 0.0f : std::pow(2.0f, 10.0f * (t - 1.0f));
    case EaseType::OutExpo:
        return (t == 1.0f) ? 1.0f : 1.0f - std::pow(2.0f, -10.0f * t);
    case EaseType::InOutExpo:
        if (t == 0.0f || t == 1.0f) return t;
        if (t < 0.5f) return 0.5f * std::pow(2.0f, (20.0f * t) - 10.0f);
        return -0.5f * std::pow(2.0f, (-20.0f * t) + 10.0f) + 1.0f;
    case EaseType::OutBounce: {
        const f32 n1 = 7.5625f;
        const f32 d1 = 2.75f;
        if (t < 1.0f / d1) return n1 * t * t;
        else if (t < 2.0f / d1) {
            t -= 1.5f / d1;
            return n1 * t * t + 0.75f;
        } else if (t < 2.5f / d1) {
            t -= 2.25f / d1;
            return n1 * t * t + 0.9375f;
        } else {
            t -= 2.625f / d1;
            return n1 * t * t + 0.984375f;
        }
    }
    case EaseType::InBounce:
        return 1.0f - Ease(EaseType::OutBounce, 1.0f - t);
    case EaseType::InOutBounce:
        return t < 0.5f
            ? (1.0f - Ease(EaseType::OutBounce, 1.0f - 2.0f * t)) * 0.5f
            : (1.0f + Ease(EaseType::OutBounce, 2.0f * t - 1.0f)) * 0.5f;
    case EaseType::OutElastic: {
        const f32 c4 = (2.0f * kPi) / 3.0f;
        return (t == 0.0f) ? 0.0f : (t == 1.0f) ? 1.0f : std::pow(2.0f, -10.0f * t) * std::sin((t * 10.0f - 0.75f) * c4) + 1.0f;
    }
    default:
        return t;
    }
}

struct Timer {
    f32  elapsed = 0.0f;
    f32  duration = 1.0f;
    bool is_looping = false;
    bool is_paused = false;

    Timer() = default;
    explicit Timer(f32 dur, bool loop = false) : duration(dur), is_looping(loop) {}

    bool Tick(f32 dt) {
        if (is_paused || duration <= 0.0f) return false;
        elapsed += dt;
        if (elapsed >= duration) {
            if (is_looping) {
                elapsed = std::fmod(elapsed, duration);
            } else {
                elapsed = duration;
            }
            return true; // Fired
        }
        return false;
    }

    f32 Progress() const {
        if (duration <= 0.0f) return 1.0f;
        return Clamp(elapsed / duration, 0.0f, 1.0f);
    }

    bool Finished() const {
        return !is_looping && elapsed >= duration;
    }

    void Reset() {
        elapsed = 0.0f;
    }
};

} // namespace lucida

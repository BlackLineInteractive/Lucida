// Lucida Engine
// Copyright (C) 2026 BlackLine Interactive
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
// Camera state, plain data. Whoever moves it (player controller, cutscene,
// benchmark script) lives outside the renderer — the renderer only reads it.

#include "lucida/core/math/Math.h"

namespace lucida {

struct CameraState {
    Vec3 position{0.0f, 0.0f, 3.0f};
    f32  yaw   = -kHalfPi;   // radians, around +Y
    f32  pitch = 0.0f;       // radians, clamped by the controller
    f32  fov_y = 60.0f * kDegToRad;

    Vec3 Forward() const {
        return Vec3(std::cos(yaw) * std::cos(pitch), std::sin(pitch),
                    std::sin(yaw) * std::cos(pitch));
    }
    Vec3 Right() const { return glm::normalize(glm::cross(Forward(), Vec3(0, 1, 0))); }
    Vec3 Up()    const { return glm::normalize(glm::cross(Right(), Forward())); }
};

} // namespace lucida

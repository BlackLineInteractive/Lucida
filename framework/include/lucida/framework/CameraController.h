// Lucida Engine
// Copyright (C) 2026 BlackLine Interactive
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
// Player camera. This logic used to live inside the Metal renderer, which is
// why it is called out here: movement, gravity and collision are gameplay, and
// a renderer that owns them cannot be swapped for another one.

#include "lucida/input/Input.h"
#include "lucida/render/Camera.h"

namespace lucida {

enum class CameraMode : u8 { Fly, Walk };

enum class ViewPreset : u8 {
    Top, Bottom, Front, Back, Right, Left, Isometric, Reset
};

struct CameraTuning {
    f32 look_sensitivity = 0.003f;
    f32 walk_speed  = 4.5f;
    f32 fly_speed   = 6.0f;
    f32 sprint_mul  = 2.2f;
    f32 gravity     = 16.0f;
    f32 jump_speed  = 5.5f;
    f32 eye_height  = 0.0f;    // standing eye level above the floor plane
    f32 crouch_drop = 0.5f;
    f32 pitch_limit = 1.5f;    // radians
    f32 fov_degrees = 60.0f;
};

class CameraController {
public:
    void Update(const InputState& input, f32 dt);

    // Fixed step for the parts that integrate: gravity and jumping.
    void FixedUpdate(const InputState& input, f32 dt);

    CameraState& Camera() { return m_camera; }
    const CameraState& Camera() const { return m_camera; }

    CameraMode Mode() const { return m_mode; }
    void SetMode(CameraMode mode);
    void ToggleMode() { SetMode(m_mode == CameraMode::Fly ? CameraMode::Walk : CameraMode::Fly); }

    CameraTuning& Tuning() { return m_tuning; }
    const CameraTuning& Tuning() const { return m_tuning; }

    void LookAt(const Vec3& eye, const Vec3& target);
    void Focus(const Vec3& target, f32 distance = 5.0f);
    void SetViewPreset(ViewPreset preset, const Vec3& target = Vec3(0.0f));
    void AdjustSpeed(f32 delta);

private:
    CameraState  m_camera;
    CameraTuning m_tuning;
    CameraMode   m_mode = CameraMode::Fly;
    f32 m_velocity_y = 0.0f;
};

} // namespace lucida

// Lucida Engine
// Copyright (C) 2026 BlackLine Interactive
// SPDX-License-Identifier: GPL-3.0-or-later
#include "lucida/framework/CameraController.h"

namespace lucida {

void CameraController::SetMode(CameraMode mode) {
    m_mode = mode;
    if (mode == CameraMode::Walk && m_camera.position.y < m_tuning.eye_height) {
        m_camera.position.y = m_tuning.eye_height;
        m_velocity_y = 0.0f;
    }
}

void CameraController::Update(const InputState& input, f32 dt) {
    // Looking is sampled per frame, not per tick: the mouse delta already is
    // the movement that happened since the last frame.
    if (input.mouse_captured) {
        m_camera.yaw   += input.mouse_delta.x * m_tuning.look_sensitivity;
        m_camera.pitch -= input.mouse_delta.y * m_tuning.look_sensitivity;
        m_camera.pitch  = Clamp(m_camera.pitch, -m_tuning.pitch_limit, m_tuning.pitch_limit);
    }

    const Vec3 forward = m_camera.Forward();
    const Vec3 right   = m_camera.Right();

    // Walking follows the ground plane, flying follows the look direction.
    const Vec3 flat_len = Vec3(forward.x, 0.0f, forward.z);
    const Vec3 flat = glm::length(flat_len) > kEpsilon ? glm::normalize(flat_len) : forward;
    const Vec3 heading = (m_mode == CameraMode::Walk) ? flat : forward;

    const Vec2 axis = input.MoveAxis();
    Vec3 move = heading * axis.y + right * axis.x;
    if (glm::length(move) > kEpsilon) move = glm::normalize(move);

    const f32 base = (m_mode == CameraMode::Walk) ? m_tuning.walk_speed : m_tuning.fly_speed;
    const f32 speed = base * (input.Down(Action::Sprint) ? m_tuning.sprint_mul : 1.0f);
    m_camera.position += move * speed * dt;

    if (m_mode == CameraMode::Fly) {
        if (input.Down(Action::Jump))   m_camera.position.y += speed * dt;
        if (input.Down(Action::Crouch)) m_camera.position.y -= speed * dt;
    }
}

void CameraController::FixedUpdate(const InputState& input, f32 dt) {
    if (m_mode != CameraMode::Walk) return;

    m_velocity_y -= m_tuning.gravity * dt;
    m_camera.position.y += m_velocity_y * dt;

    const bool crouching = input.Down(Action::Crouch);
    const f32  floor = m_tuning.eye_height - (crouching ? m_tuning.crouch_drop : 0.0f);

    if (m_camera.position.y <= floor) {
        m_camera.position.y = floor;
        m_velocity_y = 0.0f;
        if (input.Down(Action::Jump) && !crouching) m_velocity_y = m_tuning.jump_speed;
    }
}

void CameraController::LookAt(const Vec3& eye, const Vec3& target) {
    m_camera.position = eye;
    const Vec3 dir = target - eye;
    const f32 len = glm::length(dir);
    if (len > kEpsilon) {
        const Vec3 ndir = dir / len;
        m_camera.yaw   = std::atan2(ndir.z, ndir.x);
        m_camera.pitch = std::asin(Clamp(ndir.y, -0.999f, 0.999f));
    }
}

void CameraController::Focus(const Vec3& target, f32 distance) {
    const f32 dist = std::max(distance, 1.0f);
    const Vec3 fwd = m_camera.Forward();
    m_camera.position = target - fwd * dist;
}

void CameraController::SetViewPreset(ViewPreset preset, const Vec3& target) {
    const f32 dist = std::max(glm::length(m_camera.position - target), 6.0f);
    switch (preset) {
    case ViewPreset::Top:
        m_camera.position = target + Vec3(0.0f, dist, 0.001f);
        m_camera.pitch = -kHalfPi + 0.001f;
        m_camera.yaw = -kHalfPi;
        break;
    case ViewPreset::Bottom:
        m_camera.position = target - Vec3(0.0f, dist, 0.001f);
        m_camera.pitch = kHalfPi - 0.001f;
        m_camera.yaw = -kHalfPi;
        break;
    case ViewPreset::Front:
        m_camera.position = target + Vec3(0.0f, 1.5f, dist);
        LookAt(m_camera.position, target + Vec3(0.0f, 1.0f, 0.0f));
        break;
    case ViewPreset::Back:
        m_camera.position = target - Vec3(0.0f, -1.5f, dist);
        LookAt(m_camera.position, target + Vec3(0.0f, 1.0f, 0.0f));
        break;
    case ViewPreset::Right:
        m_camera.position = target + Vec3(dist, 1.5f, 0.0f);
        LookAt(m_camera.position, target + Vec3(0.0f, 1.0f, 0.0f));
        break;
    case ViewPreset::Left:
        m_camera.position = target - Vec3(dist, -1.5f, 0.0f);
        LookAt(m_camera.position, target + Vec3(0.0f, 1.0f, 0.0f));
        break;
    case ViewPreset::Isometric:
        m_camera.position = target + Vec3(dist * 0.7f, dist * 0.7f, dist * 0.7f);
        LookAt(m_camera.position, target);
        break;
    case ViewPreset::Reset:
        m_camera.position = Vec3(0.0f, 3.0f, 8.0f);
        LookAt(m_camera.position, Vec3(0.0f, 1.0f, 0.0f));
        break;
    }
}

void CameraController::AdjustSpeed(f32 delta) {
    m_tuning.fly_speed = Clamp(m_tuning.fly_speed + delta, 0.5f, 50.0f);
    m_tuning.walk_speed = Clamp(m_tuning.walk_speed + delta * 0.75f, 0.5f, 30.0f);
}

} // namespace lucida

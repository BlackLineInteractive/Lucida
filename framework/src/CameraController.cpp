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

} // namespace lucida

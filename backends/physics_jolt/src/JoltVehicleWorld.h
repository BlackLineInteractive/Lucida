// Lucida Engine
// Copyright (C) 2026 BlackLine Interactive
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
// Internal to the Jolt backend: the wheeled-vehicle world carried over from
// RayTracer_Unified. Nothing outside this directory includes it.
// ============================================================ PhysicsWorld.h
// Thin C++ wrapper around Jolt Physics that drives the 1969 Ford Mustang
// Boss 302 as a wheeled-vehicle simulation.
//
// Architecture
// ────────────
//  • PhysicsWorld::Init()  — creates Jolt systems, static ground plane, and
//    the car body rigid body with VehicleConstraint.
//  • PhysicsWorld::Step()  — advances the simulation by dt seconds,
//    reads throttle/brake/steer/handbrake from CarInput.
//  • The caller reads car_pos / car_rot / wheels[] every frame.
//  • All units are SI (metres, kg, N, rad).

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <array>

// ---------------------------------------------------------------- CarInput --
struct CarInput {
    float throttle  = 0.0f;  // 0 → 1
    float brake     = 0.0f;  // 0 → 1
    float steer     = 0.0f;  // −1 (left) → +1 (right)
    bool  handbrake = false;
    bool  reset     = false;  // teleport back to spawn
};

// --------------------------------------------------------------- WheelState -
struct WheelState {
    glm::vec3 world_pos    = glm::vec3(0.0f);
    glm::quat world_rot    = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    float steer_angle_rad  = 0.0f;
    float spin_angle_rad   = 0.0f;
};

// ------------------------------------------------------------ PhysicsWorld --
class PhysicsWorld {
public:
    PhysicsWorld()  = default;
    ~PhysicsWorld() = default;

    void Init();
    void Shutdown();
    void Step(float dt, const CarInput& input);
    void Reset();

    // ---- Outputs ----
    glm::vec3 car_pos   = glm::vec3(0.0f, 0.5f, 0.0f);
    glm::quat car_rot   = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);

    std::array<WheelState, 4> wheels;  // FL=0, FR=1, RL=2, RR=3

    float speed_kmh    = 0.0f;
    float engine_rpm   = 800.0f;
    int   current_gear = 0;

    bool  is_initialised = false;

private:
    struct Impl;
    Impl* m_impl = nullptr;
};

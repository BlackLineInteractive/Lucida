// Lucida Engine
// Copyright (C) 2026 BlackLine Interactive
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
// Character Controller System (GEA ch. 12)
//
// Each fixed tick (Play Mode only):
//   1. Read PlayerInputComponent -> desired move direction + jump flag.
//   2. Compute velocity from CharacterMovementComponent (walk/run/jump).
//   3. Call IPhysicsBackend::MoveCharacter -- the backend handles slide and step.
//   4. Read back position -> write to LocalTransform.
//   5. Write is_grounded back to CharacterBodyComponent.
//
// Entities need: CharacterBodyComponent + CharacterMovementComponent
//                + PlayerInputComponent + LocalTransform

#include "lucida/core/math/Math.h"
#include <cstdint>

namespace lucida {

class World;
class IPhysicsBackend;

class CharacterSystem {
public:
    // Called once when entering Play Mode: create physics characters for every
    // entity that has CharacterBodyComponent but no handle yet.
    void OnEnterPlay(World& world, IPhysicsBackend* physics);

    // Called once when leaving Play Mode: destroy all character handles.
    void OnExitPlay(World& world, IPhysicsBackend* physics);

    // Called each fixed tick while PlayState == Playing.
    // camera_yaw_rad: horizontal camera angle used to orient WASD in world space.
    void Update(World& world, IPhysicsBackend* physics, f32 dt, f32 camera_yaw_rad = 0.0f);
};

} // namespace lucida

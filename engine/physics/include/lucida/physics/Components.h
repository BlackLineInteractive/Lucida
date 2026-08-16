// Lucida Engine
// Copyright (C) 2026 BlackLine Interactive
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
// Physics-side components: handles into whichever backend is running.

#include "lucida/physics/PhysicsBackend.h"

namespace lucida {

struct RigidBody {
    BodyHandle body;
    BodyType   type = BodyType::Dynamic;
    ShapeType  shape = ShapeType::Box;
    f32        mass = 1.0f;
    f32        friction = 0.5f;
    f32        restitution = 0.0f;
    f32        linear_damping = 0.05f;
    f32        angular_damping = 0.05f;
    f32        gravity_scale = 1.0f;
    bool       is_active = true;
};

// A driveable vehicle. Input is written by whatever controls it - the player,
// an AI, a replay - and the backend turns it into forces.
struct Vehicle {
    VehicleHandle handle;
    VehicleInput  input;
};

} // namespace lucida

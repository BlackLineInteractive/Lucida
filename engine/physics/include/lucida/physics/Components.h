// Lucida Engine
// Copyright (C) 2026 BlackLine Interactive
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
// Physics-side components: handles into whichever backend is running.

#include "lucida/physics/PhysicsBackend.h"

namespace lucida {

struct RigidBody {
    BodyHandle body;
};

// A driveable vehicle. Input is written by whatever controls it — the player,
// an AI, a replay — and the backend turns it into forces.
struct Vehicle {
    VehicleHandle handle;
    VehicleInput  input;
};

} // namespace lucida

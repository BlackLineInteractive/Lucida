// Lucida Engine
// Copyright (C) 2026 BlackLine Interactive
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
// Update Method (GPP ch.3), applied per system rather than per object.
//
// DOD ch.4: the loop belongs to the system, not to each entity, so one call
// walks a packed array instead of chasing a virtual call per object.

#include "lucida/core/platform/Time.h"

namespace lucida {

class World;

enum class UpdatePhase : u8 {
    PreSimulation,   // input sampling, spawning
    Simulation,      // fixed step: physics, gameplay
    PostSimulation,  // resolution, culling
    Presentation     // variable step: camera, animation blending
};

class ISystem {
public:
    virtual ~ISystem() = default;

    virtual const char* Name() const = 0;
    virtual UpdatePhase Phase() const { return UpdatePhase::Simulation; }

    virtual void OnAttach(World& /*world*/) {}
    virtual void OnDetach(World& /*world*/) {}

    // Fixed step for Simulation phases, variable for Presentation.
    virtual void Update(World& world, const FrameTime& time) = 0;
};

} // namespace lucida

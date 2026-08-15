// Lucida Engine
// Copyright (C) 2026 BlackLine Interactive
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
// Application callbacks. The runtime owns the loop; the app owns the game.
//
// Note what is missing: no window, no renderer, no physics types. The runtime
// links only lucida::core, so the app is what binds concrete backends together
// (ARCHITECTURE.md, rule 4).

#include "lucida/core/platform/Time.h"

namespace lucida {

class World;

class IApplication {
public:
    virtual ~IApplication() = default;

    virtual bool OnInit(World& world) = 0;
    virtual void OnShutdown(World& world) = 0;

    // Pump OS events, sample input. Return false to quit.
    virtual bool OnPollEvents(World& world) = 0;

    // Fixed step, may run zero or several times per frame.
    virtual void OnFixedUpdate(World& world, const FrameTime& time) = 0;

    // Once per frame, variable step. alpha in time is the interpolation factor.
    virtual void OnRender(World& world, const FrameTime& time) = 0;
};

} // namespace lucida

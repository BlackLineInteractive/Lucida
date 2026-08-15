// Lucida Engine
// Copyright (C) 2026 BlackLine Interactive
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
// The two systems that keep physics, entities and the renderer in step.
//
// They exist so nothing else has to remember the order. Physics writes
// transforms, the world resolves hierarchy, the renderer reads the result —
// once per frame, in that order, in one place.

#include "lucida/physics/PhysicsBackend.h"
#include "lucida/render/RenderBackend.h"
#include "lucida/runtime/System.h"

namespace lucida {

// Fixed step. Advances the simulation and writes each vehicle's pose onto its
// entity, so gameplay reads a transform rather than a physics handle.
class PhysicsSystem final : public ISystem {
public:
    explicit PhysicsSystem(IPhysicsBackend& physics) : m_physics(physics) {}

    const char* Name() const override { return "physics"; }
    UpdatePhase Phase() const override { return UpdatePhase::Simulation; }
    void Update(World& world, const FrameTime& time) override;

private:
    IPhysicsBackend& m_physics;
};

// Presentation step. Pushes world transforms to the backend, and only for
// instances that actually moved: an unchanged matrix costs an upload otherwise.
class RenderSyncSystem final : public ISystem {
public:
    explicit RenderSyncSystem(IRenderBackend& renderer) : m_renderer(renderer) {}

    const char* Name() const override { return "render-sync"; }
    UpdatePhase Phase() const override { return UpdatePhase::Presentation; }
    void Update(World& world, const FrameTime& time) override;

private:
    IRenderBackend& m_renderer;
};

} // namespace lucida

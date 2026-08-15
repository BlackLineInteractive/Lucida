// Lucida Engine
// Copyright (C) 2026 BlackLine Interactive
// SPDX-License-Identifier: GPL-3.0-or-later
#include "lucida/framework/Systems.h"

#include "lucida/core/diag/Profiler.h"
#include "lucida/physics/Components.h"
#include "lucida/render/Components.h"
#include "lucida/runtime/World.h"

#include <cstring>

namespace lucida {

void PhysicsSystem::Update(World& world, const FrameTime& time) {
    LUCIDA_PROFILE("physics");

    Registry& entities = world.Entities();

    // Intent in, before the step: a vehicle's input has to be applied to the
    // simulation it is about to run, not to the one that already ran.
    for (auto [entity, vehicle] : entities.View<Vehicle>().each()) {
        if (vehicle.handle.IsValid()) m_physics.SetVehicleInput(vehicle.handle, vehicle.input);
    }

    m_physics.Step(time.delta);

    // Poses out. Physics owns the transform of anything it simulates, so this
    // writes rather than blends: a gameplay system that also moved the entity
    // this tick would be fighting the solver, and the solver would win anyway.
    for (auto [entity, vehicle, local] : entities.View<Vehicle, LocalTransform>().each()) {
        if (!vehicle.handle.IsValid()) continue;
        const VehicleState state = m_physics.GetVehicleState(vehicle.handle);
        local.position = state.position;
        local.rotation = state.rotation;
    }
}

void RenderSyncSystem::Update(World& world, const FrameTime& time) {
    LUCIDA_PROFILE("render-sync");
    (void)time;

    for (auto [entity, mesh, world_transform, visibility] :
         world.Entities().View<MeshInstance, WorldTransform, Visibility>().each()) {
        if (!mesh.instance.IsValid() || !visibility.visible) continue;
        m_renderer.SetInstanceTransform(mesh.instance, world_transform.matrix);
    }
}

} // namespace lucida

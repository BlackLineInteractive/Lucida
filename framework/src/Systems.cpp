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
    Registry& entities = world.Entities();

    // 1. Ensure all RigidBody components have valid physics bodies
    for (auto [entity, rb, local] : entities.View<RigidBody, LocalTransform>().each()) {
        if (!rb.body.IsValid() && rb.is_active) {
            BodyDesc desc;
            desc.type = rb.type;
            desc.shape = rb.shape;
            desc.position = local.position;
            desc.rotation = local.rotation;
            desc.mass = rb.mass;
            desc.friction = rb.friction;
            desc.restitution = rb.restitution;

            // Determine shape size from PrimitiveShape, LocalBounds, or fallback
            if (const PrimitiveShape* ps = entities.Get<PrimitiveShape>(entity)) {
                desc.half_extent = ps->HalfExtents() * local.scale;
                if (ps->type == PrimitiveType::Sphere)        desc.shape = ShapeType::Sphere;
                else if (ps->type == PrimitiveType::Box)      desc.shape = ShapeType::Box;
                else if (ps->type == PrimitiveType::Cylinder) desc.shape = ShapeType::Cylinder;
                else if (ps->type == PrimitiveType::Cone)     desc.shape = ShapeType::Cylinder;
                else if (ps->type == PrimitiveType::Plane) {
                    desc.shape = ShapeType::Box;
                    desc.type = BodyType::Static;
                }
            } else if (const LocalBounds* lb = entities.Get<LocalBounds>(entity)) {
                desc.half_extent = (lb->max - lb->min) * 0.5f * local.scale;
                desc.shape = ShapeType::Box;
            } else {
                desc.half_extent = local.scale * 0.5f;
            }

            rb.body = m_physics.CreateBody(desc);
        } else if (rb.body.IsValid() && m_paused) {
            // While editing / paused, sync entity transform changes to the physics body
            m_physics.SetBodyTransform(rb.body, Transform{local.position, local.rotation, local.scale.x});
        }
    }

    if (m_paused) return;
    LUCIDA_PROFILE("physics");

    // 2. Drive Vehicles
    for (auto [entity, vehicle] : entities.View<Vehicle>().each()) {
        if (vehicle.handle.IsValid()) m_physics.SetVehicleInput(vehicle.handle, vehicle.input);
    }

    // 3. Step simulation
    m_physics.Step(time.delta);

    // 4. Update Vehicle poses
    for (auto [entity, vehicle, local] : entities.View<Vehicle, LocalTransform>().each()) {
        if (!vehicle.handle.IsValid()) continue;
        const VehicleState state = m_physics.GetVehicleState(vehicle.handle);
        local.position = state.position;
        local.rotation = state.rotation;
    }

    // 5. Update RigidBody poses (physics -> entity)
    for (auto [entity, rb, local] : entities.View<RigidBody, LocalTransform>().each()) {
        if (!rb.body.IsValid() || rb.type == BodyType::Static) continue;
        const Transform t = m_physics.GetBodyTransform(rb.body);
        local.position = t.position;
        local.rotation = t.rotation;
    }
}

void RenderSyncSystem::Update(World& world, const FrameTime& time) {
    LUCIDA_PROFILE("render-sync");
    (void)time;
    if (!m_renderer) return;

    Registry& entities = world.Entities();
    auto view = entities.View<MeshInstance, WorldTransform, Visibility>();

    u64 topology = 0;
    size_t count = 0;
    for (auto [entity, mesh, world_transform, visibility] : view.each()) {
        if (!visibility.visible || !mesh.mesh.IsValid()) continue;
        topology ^= u64(entt::to_integral(entity)) * 0x9e3779b97f4a7c15ULL;
        topology ^= u64(mesh.mesh.index) * 0x517cc1b727220a95ULL;
        count++;
    }

    if (topology != m_last_topology || count != m_last_mesh_count) {
        m_last_topology = topology;
        m_last_mesh_count = count;
        m_renderer->ClearInstances();
        for (auto [entity, mesh, world_transform, visibility] : view.each()) {
            if (!mesh.mesh.IsValid() || !visibility.visible) {
                mesh.instance = InstanceHandle{};
                continue;
            }
            mesh.instance = m_renderer->AddInstance(mesh.mesh, world_transform.matrix);
        }
    } else {
        for (auto [entity, mesh, world_transform, visibility] : view.each()) {
            if (!mesh.instance.IsValid() || !visibility.visible) continue;
            m_renderer->SetInstanceTransform(mesh.instance, world_transform.matrix);
        }
    }
}

} // namespace lucida

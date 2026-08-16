// Lucida Engine
// Copyright (C) 2026 BlackLine Interactive
// SPDX-License-Identifier: GPL-3.0-or-later
#include "lucida/framework/Systems.h"

#include "lucida/core/diag/Profiler.h"
#include "lucida/physics/Components.h"
#include "lucida/render/Components.h"
#include "lucida/runtime/World.h"

#include "lucida/framework/Script.h"

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
            desc.is_sensor = rb.is_trigger;
            desc.user_data = static_cast<uint32_t>(entity);

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

    // 4. Dispatch Collision & Trigger Events to Scripts
    std::vector<CollisionEvent> events;
    m_physics.PopCollisionEvents(events);
    for (const auto& ev : events) {
        Entity e_a = static_cast<Entity>(ev.user_data_a);
        Entity e_b = static_cast<Entity>(ev.user_data_b);

        if (ScriptComponent* sc_a = entities.Get<ScriptComponent>(e_a)) {
            for (auto& script : sc_a->scripts) {
                if (ev.type == CollisionEvent::Type::Begin) {
                    script->OnCollisionEnter(world, e_a, e_b, ev.contact);
                } else if (ev.type == CollisionEvent::Type::End) {
                    script->OnCollisionExit(world, e_a, e_b);
                } else if (ev.type == CollisionEvent::Type::TriggerEnter) {
                    script->OnTriggerEnter(world, e_a, e_b);
                } else if (ev.type == CollisionEvent::Type::TriggerExit) {
                    script->OnTriggerExit(world, e_a, e_b);
                }
            }
        }

        if (ScriptComponent* sc_b = entities.Get<ScriptComponent>(e_b)) {
            for (auto& script : sc_b->scripts) {
                if (ev.type == CollisionEvent::Type::Begin) {
                    script->OnCollisionEnter(world, e_b, e_a, ev.contact);
                } else if (ev.type == CollisionEvent::Type::End) {
                    script->OnCollisionExit(world, e_b, e_a);
                } else if (ev.type == CollisionEvent::Type::TriggerEnter) {
                    script->OnTriggerEnter(world, e_b, e_a);
                } else if (ev.type == CollisionEvent::Type::TriggerExit) {
                    script->OnTriggerExit(world, e_b, e_a);
                }
            }
        }
    }

    // 5. Update Vehicle poses
    for (auto [entity, vehicle, local] : entities.View<Vehicle, LocalTransform>().each()) {
        if (!vehicle.handle.IsValid()) continue;
        const VehicleState state = m_physics.GetVehicleState(vehicle.handle);
        local.position = state.position;
        local.rotation = state.rotation;
    }

    // 6. Update RigidBody poses (physics -> entity)
    for (auto [entity, rb, local] : entities.View<RigidBody, LocalTransform>().each()) {
        if (!rb.body.IsValid() || rb.type == BodyType::Static) continue;
        const Transform t = m_physics.GetBodyTransform(rb.body);
        local.position = t.position;
        local.rotation = t.rotation;
    }
}

void ScriptSystem::Update(World& world, const FrameTime& time) {
    if (m_paused) return;
    LUCIDA_PROFILE("script");

    Registry& entities = world.Entities();
    for (auto [entity, sc] : entities.View<ScriptComponent>().each()) {
        for (auto& script : sc.scripts) {
            if (!script->is_started) {
                script->OnStart(world, entity);
                script->is_started = true;
            }
            script->OnUpdate(world, entity, time.delta);
        }
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

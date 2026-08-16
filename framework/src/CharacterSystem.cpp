// Lucida Engine
// Copyright (C) 2026 BlackLine Interactive
// SPDX-License-Identifier: GPL-3.0-or-later
#include "lucida/framework/CharacterSystem.h"

#include "lucida/runtime/GameplayComponents.h"
#include "lucida/runtime/World.h"
#include "lucida/physics/PhysicsBackend.h"
#include "lucida/core/ecs/Registry.h"
#include "lucida/core/diag/Log.h"

#include <cmath>
#include <climits>

namespace lucida {

void CharacterSystem::OnEnterPlay(World& world, IPhysicsBackend* physics) {
    if (!physics) return;
    Registry& reg = world.Entities();
    auto view = reg.Raw().view<CharacterBodyComponent, LocalTransform>();
    for (auto entity : view) {
        auto& cbc = view.get<CharacterBodyComponent>(entity);
        if (cbc.physics_handle != UINT32_MAX) continue; // already created

        auto& lt = view.get<LocalTransform>(entity);
        CharacterDesc desc{};
        desc.position       = lt.position;
        desc.capsule_radius = cbc.capsule_radius;
        desc.capsule_height = cbc.capsule_height;
        desc.step_height    = cbc.step_height;
        desc.max_slope_deg  = cbc.max_slope_angle_deg;

        CharacterHandle ch = physics->CreateCharacter(desc);
        if (ch.IsValid()) {
            cbc.physics_handle = ch.index;
            LUCIDA_INFO(Runtime, "CharacterSystem: created handle %u for entity %u",
                        ch.index, (u32)entity);
        } else {
            LUCIDA_WARN(Runtime, "CharacterSystem: failed to create character for entity %u",
                        (u32)entity);
        }
    }
}

void CharacterSystem::OnExitPlay(World& world, IPhysicsBackend* physics) {
    if (!physics) return;
    Registry& reg = world.Entities();
    auto view = reg.Raw().view<CharacterBodyComponent>();
    for (auto entity : view) {
        auto& cbc = view.get<CharacterBodyComponent>(entity);
        if (cbc.physics_handle == UINT32_MAX) continue;
        physics->DestroyCharacter(CharacterHandle{cbc.physics_handle, 1});
        cbc.physics_handle = UINT32_MAX;
        cbc.is_grounded    = false;
    }
}

void CharacterSystem::Update(World& world, IPhysicsBackend* physics,
                             f32 dt, f32 camera_yaw_rad) {
    if (!physics || dt <= 0.0f) return;
    Registry& reg = world.Entities();

    // Entities with full controller stack
    auto view = reg.Raw().view<CharacterBodyComponent,
                                CharacterMovementComponent,
                                PlayerInputComponent,
                                LocalTransform>();

    for (auto entity : view) {
        auto& cbc  = view.get<CharacterBodyComponent>(entity);
        auto& cmc  = view.get<CharacterMovementComponent>(entity);
        auto& pic  = view.get<PlayerInputComponent>(entity);
        auto& lt   = view.get<LocalTransform>(entity);

        if (cbc.physics_handle == UINT32_MAX) continue;
        CharacterHandle ch{cbc.physics_handle, 1};

        // --- Build world-space desired velocity ---
        // Rotate move axis by camera yaw so WASD follows camera facing
        float sin_yaw = std::sin(camera_yaw_rad);
        float cos_yaw = std::cos(camera_yaw_rad);
        Vec3 forward{ sin_yaw, 0.0f,  cos_yaw};
        Vec3 right  { cos_yaw, 0.0f, -sin_yaw};

        float speed = pic.sprint_pressed ? cmc.run_speed : cmc.walk_speed;
        Vec3  desired = (forward * pic.move_axis.y + right * pic.move_axis.x) * speed;

        // Vertical: keep existing Y, apply jump on grounded
        desired.y = cmc.velocity.y;
        if (cbc.is_grounded) {
            desired.y = 0.0f;
            if (pic.jump_pressed) {
                desired.y = cmc.jump_force;
            }
        } else {
            // Simple gravity accumulation (9.81 m/s^2)
            desired.y -= 9.81f * dt;
        }
        desired.y = glm::clamp(desired.y, -40.0f, cmc.jump_force * 2.0f);
        cmc.velocity = desired;

        // --- Drive the capsule ---
        physics->MoveCharacter(ch, desired, dt);

        // --- Read back and sync LocalTransform ---
        Vec3 new_pos          = physics->GetCharacterPosition(ch);
        lt.position           = new_pos;
        cbc.is_grounded       = physics->IsCharacterGrounded(ch);

        // Clear jump flag so it doesn't repeat
        pic.jump_pressed = false;
    }
}

} // namespace lucida

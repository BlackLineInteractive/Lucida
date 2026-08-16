// Lucida Engine
// Copyright (C) 2026 BlackLine Interactive
// SPDX-License-Identifier: GPL-3.0-or-later
#include "lucida/runtime/Particles.h"
#include "lucida/render/Components.h"
#include "lucida/runtime/World.h"

#include <cmath>
#include <cstdlib>

namespace lucida {

void ParticleSimulationSystem::Update(World& world, const FrameTime& time) {
    if (m_paused) return;

    Registry& entities = world.Entities();
    const f32 dt = time.delta;
    if (dt <= 0.0f) return;

    for (auto [entity, emitter, local] : entities.View<ParticleEmitterComponent, LocalTransform>().each()) {
        emitter.EnsureCapacity();

        // 1. Update and simulate active particles (SoA continuous buffer iteration)
        u32 i = 0;
        while (i < emitter.active_count) {
            emitter.lifetimes[i] += dt;
            if (emitter.lifetimes[i] >= emitter.max_lifetimes[i]) {
                // O(1) removal by swapping with last active particle
                const u32 last = emitter.active_count - 1;
                if (i != last) {
                    emitter.positions[i]     = emitter.positions[last];
                    emitter.velocities[i]    = emitter.velocities[last];
                    emitter.lifetimes[i]     = emitter.lifetimes[last];
                    emitter.max_lifetimes[i] = emitter.max_lifetimes[last];
                    emitter.sizes[i]         = emitter.sizes[last];
                    emitter.colors[i]        = emitter.colors[last];
                }
                emitter.active_count--;
            } else {
                const f32 t = emitter.lifetimes[i] / emitter.max_lifetimes[i];
                emitter.velocities[i] += emitter.gravity * dt;
                emitter.positions[i]  += emitter.velocities[i] * dt;
                emitter.sizes[i]      = glm::mix(emitter.size_start, emitter.size_end, t);
                emitter.colors[i]     = glm::mix(emitter.color_start, emitter.color_end, t);
                ++i;
            }
        }

        // 2. Emit new particles
        if (emitter.is_active && emitter.emission_rate > 0.0f) {
            emitter.emit_accumulator += emitter.emission_rate * dt;
            const u32 to_spawn = static_cast<u32>(emitter.emit_accumulator);
            emitter.emit_accumulator -= static_cast<f32>(to_spawn);

            for (u32 s = 0; s < to_spawn && emitter.active_count < emitter.max_particles; ++s) {
                const u32 idx = emitter.active_count++;
                emitter.positions[idx] = local.position;
                emitter.lifetimes[idx] = 0.0f;

                const f32 r_life = static_cast<f32>(std::rand()) / static_cast<f32>(RAND_MAX);
                emitter.max_lifetimes[idx] = glm::mix(emitter.lifetime_min, emitter.lifetime_max, r_life);

                const f32 r_spd = static_cast<f32>(std::rand()) / static_cast<f32>(RAND_MAX);
                const f32 speed = glm::mix(emitter.speed_min, emitter.speed_max, r_spd);

                const f32 theta = (static_cast<f32>(std::rand()) / static_cast<f32>(RAND_MAX)) * 2.0f * kPi;
                const f32 phi   = (static_cast<f32>(std::rand()) / static_cast<f32>(RAND_MAX)) * emitter.spread_angle;

                Vec3 dir = glm::normalize(emitter.direction);
                Vec3 right = (std::abs(dir.y) < 0.99f) ? glm::normalize(glm::cross(dir, Vec3(0.0f, 1.0f, 0.0f))) : Vec3(1.0f, 0.0f, 0.0f);
                Vec3 up    = glm::cross(right, dir);
                Vec3 spread_dir = glm::normalize(dir + (right * std::cos(theta) + up * std::sin(theta)) * std::tan(phi));

                emitter.velocities[idx] = spread_dir * speed;
                emitter.sizes[idx]      = emitter.size_start;
                emitter.colors[idx]     = emitter.color_start;
            }
        }
    }
}

} // namespace lucida

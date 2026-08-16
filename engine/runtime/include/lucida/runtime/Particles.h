// Lucida Engine
// Copyright (C) 2026 BlackLine Interactive
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "lucida/core/math/Math.h"
#include "lucida/runtime/System.h"

#include <vector>

namespace lucida {

struct ParticleEmitterComponent {
    u32  max_particles   = 256;
    f32  emission_rate   = 40.0f; // particles per second
    f32  lifetime_min    = 0.8f;
    f32  lifetime_max    = 1.6f;
    f32  speed_min       = 1.5f;
    f32  speed_max       = 3.5f;
    Vec3 direction       = Vec3(0.0f, 1.0f, 0.0f);
    f32  spread_angle    = 25.0f * kDegToRad;
    Vec3 gravity         = Vec3(0.0f, -4.9f, 0.0f);
    f32  size_start      = 0.25f;
    f32  size_end        = 0.0f;
    Vec4 color_start     = Vec4(1.0f, 0.85f, 0.2f, 1.0f);
    Vec4 color_end       = Vec4(0.9f, 0.15f, 0.05f, 0.0f);
    bool is_active       = true;

    // SoA (Struct of Arrays) particle buffers for continuous memory streaming (DOD)
    std::vector<Vec3> positions;
    std::vector<Vec3> velocities;
    std::vector<f32>  lifetimes;
    std::vector<f32>  max_lifetimes;
    std::vector<f32>  sizes;
    std::vector<Vec4> colors;
    u32 active_count     = 0;

    f32 emit_accumulator = 0.0f;

    void EnsureCapacity() {
        if (positions.size() < max_particles) {
            positions.resize(max_particles);
            velocities.resize(max_particles);
            lifetimes.resize(max_particles);
            max_lifetimes.resize(max_particles);
            sizes.resize(max_particles);
            colors.resize(max_particles);
        }
    }
};

class ParticleSimulationSystem final : public ISystem {
public:
    const char* Name() const override { return "particle-sim"; }
    UpdatePhase Phase() const override { return UpdatePhase::Simulation; }
    void Update(World& world, const FrameTime& time) override;

    void SetPaused(bool paused) { m_paused = paused; }
    bool IsPaused() const { return m_paused; }

private:
    bool m_paused = false;
};

} // namespace lucida

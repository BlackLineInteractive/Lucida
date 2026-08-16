// Lucida Engine
// Copyright (C) 2026 BlackLine Interactive
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "lucida/animation/AnimationClip.h"
#include "lucida/animation/Skeleton.h"
#include "lucida/runtime/System.h"

#include <memory>
#include <vector>

namespace lucida {

struct AnimatorComponent {
    std::shared_ptr<Skeleton>      skeleton;
    std::shared_ptr<AnimationClip> current_clip;

    f32  current_time   = 0.0f;
    f32  playback_speed = 1.0f;
    bool is_looping     = true;
    bool is_playing     = true;

    // Skinning matrix palette computed for vertex deformation on GPU:
    // palette[j] = model_pose[j] * inv_bind_matrix[j]
    std::vector<Mat4> skinning_palette;
};

class AnimationSystem final : public ISystem {
public:
    const char* Name() const override { return "animation"; }
    UpdatePhase Phase() const override { return UpdatePhase::Simulation; }
    void Update(World& world, const FrameTime& time) override;

    void SetPaused(bool paused) { m_paused = paused; }
    bool IsPaused() const { return m_paused; }

private:
    bool m_paused = false;
};

} // namespace lucida

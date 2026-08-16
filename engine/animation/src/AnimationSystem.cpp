// Lucida Engine
// Copyright (C) 2026 BlackLine Interactive
// SPDX-License-Identifier: GPL-3.0-or-later
#include "lucida/animation/AnimationSystem.h"
#include "lucida/runtime/World.h"

#include <cmath>

namespace lucida {

void AnimationSystem::Update(World& world, const FrameTime& time) {
    if (m_paused) return;

    Registry& entities = world.Entities();

    for (auto [entity, anim] : entities.View<AnimatorComponent>().each()) {
        if (!anim.skeleton || !anim.current_clip || !anim.is_playing) continue;

        const auto& skel = *anim.skeleton;
        const auto& clip = *anim.current_clip;
        const usize joint_count = skel.JointCount();
        if (joint_count == 0) continue;

        // Advance playback time
        anim.current_time += time.delta * anim.playback_speed;
        if (clip.duration > 0.0f) {
            if (anim.is_looping) {
                anim.current_time = std::fmod(anim.current_time, clip.duration);
                if (anim.current_time < 0.0f) anim.current_time += clip.duration;
            } else if (anim.current_time >= clip.duration) {
                anim.current_time = clip.duration;
                anim.is_playing = false;
            }
        }

        // Allocate palette buffer
        if (anim.skinning_palette.size() != joint_count) {
            anim.skinning_palette.resize(joint_count, Mat4(1.0f));
        }

        std::vector<Mat4> model_poses(joint_count, Mat4(1.0f));

        // Evaluate skeletal hierarchy from root to leaves
        for (usize j = 0; j < joint_count; ++j) {
            const Joint& joint = skel.joints[j];
            Transform local_t = joint.bind_pose;

            if (const JointTrack* track = clip.FindTrack(static_cast<i32>(j))) {
                local_t = track->Evaluate(anim.current_time);
            }

            const Mat4 local_mat = local_t.ToMatrix();

            if (joint.parent_index >= 0 && static_cast<usize>(joint.parent_index) < joint_count) {
                model_poses[j] = model_poses[joint.parent_index] * local_mat;
            } else {
                model_poses[j] = local_mat;
            }

            // Final skinning matrix
            anim.skinning_palette[j] = model_poses[j] * joint.inv_bind_matrix;
        }
    }
}

} // namespace lucida

// Lucida Engine
// Copyright (C) 2026 BlackLine Interactive
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "lucida/core/math/Math.h"

#include <string>
#include <vector>

namespace lucida {

template <typename T>
struct Keyframe {
    f32 time = 0.0f; // in seconds
    T   value{};
};

struct JointTrack {
    i32 joint_index = -1;
    std::vector<Keyframe<Vec3>> translation_keys;
    std::vector<Keyframe<Quat>> rotation_keys;
    std::vector<Keyframe<Vec3>> scale_keys;

    Transform Evaluate(f32 time) const {
        Transform result;

        // Translation
        if (!translation_keys.empty()) {
            if (translation_keys.size() == 1 || time <= translation_keys.front().time) {
                result.position = translation_keys.front().value;
            } else if (time >= translation_keys.back().time) {
                result.position = translation_keys.back().value;
            } else {
                for (usize i = 0; i < translation_keys.size() - 1; ++i) {
                    if (time >= translation_keys[i].time && time <= translation_keys[i + 1].time) {
                        f32 t = (time - translation_keys[i].time) / (translation_keys[i + 1].time - translation_keys[i].time);
                        result.position = glm::mix(translation_keys[i].value, translation_keys[i + 1].value, t);
                        break;
                    }
                }
            }
        }

        // Rotation
        if (!rotation_keys.empty()) {
            if (rotation_keys.size() == 1 || time <= rotation_keys.front().time) {
                result.rotation = rotation_keys.front().value;
            } else if (time >= rotation_keys.back().time) {
                result.rotation = rotation_keys.back().value;
            } else {
                for (usize i = 0; i < rotation_keys.size() - 1; ++i) {
                    if (time >= rotation_keys[i].time && time <= rotation_keys[i + 1].time) {
                        f32 t = (time - rotation_keys[i].time) / (rotation_keys[i + 1].time - rotation_keys[i].time);
                        result.rotation = glm::slerp(rotation_keys[i].value, rotation_keys[i + 1].value, t);
                        break;
                    }
                }
            }
        }

        // Scale
        if (!scale_keys.empty()) {
            if (scale_keys.size() == 1 || time <= scale_keys.front().time) {
                result.scale = scale_keys.front().value.x;
            } else if (time >= scale_keys.back().time) {
                result.scale = scale_keys.back().value.x;
            } else {
                for (usize i = 0; i < scale_keys.size() - 1; ++i) {
                    if (time >= scale_keys[i].time && time <= scale_keys[i + 1].time) {
                        f32 t = (time - scale_keys[i].time) / (scale_keys[i + 1].time - scale_keys[i].time);
                        result.scale = glm::mix(scale_keys[i].value.x, scale_keys[i + 1].value.x, t);
                        break;
                    }
                }
            }
        }

        return result;
    }
};

struct AnimationClip {
    std::string name;
    f32 duration = 0.0f; // in seconds
    std::vector<JointTrack> tracks;

    const JointTrack* FindTrack(i32 joint_index) const {
        for (const auto& track : tracks) {
            if (track.joint_index == joint_index) return &track;
        }
        return nullptr;
    }
};

} // namespace lucida

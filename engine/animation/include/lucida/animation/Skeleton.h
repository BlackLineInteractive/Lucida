// Lucida Engine
// Copyright (C) 2026 BlackLine Interactive
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
// Skeletal animation data structures (GEA ch.12 Animation Systems).

#include "lucida/core/math/Math.h"

#include <string>
#include <vector>

namespace lucida {

struct Joint {
    std::string name;
    i32         parent_index = -1; // -1 indicates root joint
    Transform   bind_pose;
    Mat4        inv_bind_matrix{1.0f};
};

struct Skeleton {
    std::vector<Joint> joints;

    i32 FindJointIndex(const std::string& name) const {
        for (usize i = 0; i < joints.size(); ++i) {
            if (joints[i].name == name) return static_cast<i32>(i);
        }
        return -1;
    }

    usize JointCount() const { return joints.size(); }
};

} // namespace lucida
